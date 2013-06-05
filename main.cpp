#include <windows.h>
#include <stdio.h>
#include <Tlhelp32.h>

struct OffsetInfo
{
	char* lpszName;
	DWORD dwOffset;
	BYTE* lpbyData;
	DWORD dwLen;			

	BYTE* lpbyUpdateData;
	char* lpszMask;
}

/*g_sPatchList[] =
{
	{"DUkie zoom unbind", 0,(BYTE*)"\x90\x90\x90",3,
	(BYTE*)"\x0F\x28\xC1\x0F\x2E\xC2\x9F\xF6\xC4",
	"xxxxxxxxx"},
};*/

/*g_sPatchList[] =
{
	{"DUkie zoom unbind", 0,(BYTE*)"\x90\x90",2,
	(BYTE*)"\x7B\xF3\x0F\x11\x85\xF3\x0F\x10\x05",
	"x?xxxx????xxxx"},
};*/

g_sPatchList[] =
{
	{"DUkie zoom unbind", 0,(BYTE*)"\x90\x90\x90",3,
	(BYTE*)"\x0F\x28\xC1\xF3\x0F\x11\x05\x59",
	"xxxxxxx????x"},
	{"DUkie zoom unbind", 0,(BYTE*)"\x90\x90\x90",3,
	(BYTE*)"\x0F\x28\xC1\x0F\x2E\xC2\x9F\xF6\xC4",
	"xxxxxxxxx"},
};

/* 83799c */ 
/* 8369E9 */
/*450ca000 
3e56348
3e56350
current data segment 01868AA4*/
 

LPMODULEENTRY32 GetModuleInfo(DWORD dwPid)
{
	static MODULEENTRY32 s_sModule; 
	HANDLE hSnapshot;

	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,dwPid);
	if( hSnapshot == INVALID_HANDLE_VALUE )
	{
		printf("CreateToolhelp32Snapshot failed: %i\n",GetLastError());
		return NULL;
	}

	s_sModule.dwSize = sizeof(MODULEENTRY32);
	if( Module32First(hSnapshot,&s_sModule) == FALSE )
	{
		CloseHandle(hSnapshot);
		printf("Module32First failed: %i\n",GetLastError());
		return NULL;
	}

	do
	{
		if( strcmp("League of Legends.exe",s_sModule.szModule) == 0 )
		{
			CloseHandle(hSnapshot);
			return &s_sModule;
		}
	}while(Module32Next(hSnapshot,&s_sModule));

	printf("Couldn't find League of Legends module!\n");
	CloseHandle(hSnapshot);
	return NULL;
}

void SetDebugPrivilege()
{
	HANDLE Htoken;
	TOKEN_PRIVILEGES tokprivls;
	if(OpenProcessToken( GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &Htoken))
	{
		tokprivls.PrivilegeCount = 1;
		LookupPrivilegeValue(NULL, "SeDebugPrivilege", &tokprivls.Privileges[0].Luid);
		tokprivls.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		AdjustTokenPrivileges( Htoken, FALSE, &tokprivls, sizeof(tokprivls), NULL, NULL);
		CloseHandle(Htoken);
	}
}


void FindOffset(HANDLE hProcess, LPMODULEENTRY32 lpsModule, int iNumber)
{
	bool bFound;
	DWORD dwLen,dwRead, dwAdjust, dwParser;
	BYTE* lpbyBuffer = new BYTE[lpsModule->modBaseSize];

	dwLen = strlen(g_sPatchList[iNumber].lpszMask);
	ReadProcessMemory(hProcess,(void*)lpsModule->modBaseAddr,(void*)lpbyBuffer,lpsModule->modBaseSize,&dwRead);

	for( DWORD i = 0; i < dwRead; ++i )
	{
		if( g_sPatchList[iNumber].lpbyUpdateData[0] == lpbyBuffer[i] )
		{
			bFound = true;
			dwAdjust = 0;
			dwParser = 1;
			for( DWORD a = 1; a < dwLen; ++a )
			{
				//Skip INT3 between functions
				while( lpbyBuffer[i+a+dwAdjust] == 0xCC )
					++dwAdjust;

				if( g_sPatchList[iNumber].lpszMask[a] == 'x' && g_sPatchList[iNumber].lpbyUpdateData[dwParser++] != lpbyBuffer[i+a+dwAdjust] )
				{
					bFound = false;
					break;
				}
			}
			if( bFound == true )
			{
				g_sPatchList[iNumber].dwOffset = (DWORD)lpsModule->modBaseAddr + i;
				delete[]lpbyBuffer;
				return;
			}
		}
	}
	delete[]lpbyBuffer;
}

int main()
{
	HWND hWindow = 0,hWindowOld = 0;
	DWORD dwWritten, dwPid;
	HANDLE hProcess;
	LPMODULEENTRY32 lpsModule;

	SetConsoleTitle("Steam");
	SetDebugPrivilege();
	while(1)
	{
		printf("Zhdem nachala igri\n");
		while( (hWindow = FindWindow(NULL, "League of Legends (TM) Client")) == 0 || hWindow == hWindowOld )
			Sleep(1000);

		hWindowOld = hWindow;
		GetWindowThreadProcessId(hWindow, &dwPid);
		hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwPid);
		if( hProcess == NULL )
		{
			printf("Couldn't get client handle!\n\n");
			continue;
		}
		printf("Nashel!\n");
		lpsModule = GetModuleInfo(dwPid);
		for( int i = 0; i < sizeof(g_sPatchList)/sizeof(OffsetInfo); ++i )
		{
			if( g_sPatchList[i].dwOffset == 0)
			{
				if( lpsModule )
					FindOffset(hProcess, lpsModule, i);
			}

			if( g_sPatchList[i].dwOffset )
			{
				WriteProcessMemory(hProcess,(void*)g_sPatchList[i].dwOffset,(void*)g_sPatchList[i].lpbyData,g_sPatchList[i].dwLen,&dwWritten);
				printf("GOTOVO! Polozhil Podorozhnik na adres: %X\n",g_sPatchList[i].dwOffset);
			}else
				printf("NOT Patched: %s\n",g_sPatchList[i].lpszName);
		}
		CloseHandle(hProcess);
		printf("\n");
	}
	
	return 0;
}
