#include "resource.h"
#include "lua_main.h"
#include "ctime"

///////////////////
// Plugin Variables
///////////////////
NppData nppData;
ExecData execData;
CPluginOptions g_opt;
bool bNeedClear = false;
tTbData dockingData;
SCNotification *scNotification;
HMODULE m_hRichEditDll;
CLuaManager* LM;

TCHAR path_lua51[] = L"lua51.dll";
TCHAR path_lua52[] = L"lua52.dll";
TCHAR path_lua53[] = L"lua53.dll";
TCHAR path_lua54[] = L"lua54.dll";
TCHAR path_luajit[] = L"luajit.dll";
TCHAR ConsoleCaption[20]{};
BYTE is_floating = 8;

WNDPROC OriginalRichEditProc = NULL;
LRESULT CALLBACK RichEditWndProc(
	HWND   hEd, 
	UINT   uMessage, 
	WPARAM wParam, 
	LPARAM lParam)
{
	if ( uMessage == WM_LBUTTONDBLCLK ) OpenCloseConsole();
	return OriginalRichEditProc(hEd, uMessage, wParam, lParam);
};

FuncItem funcItems[nbFunc];

bool file_exist(const TCHAR* dll_name)
{
	TCHAR lua_dll[MAX_PATH]{ 0 };
	UINT nLen = GetModuleFileName((HMODULE)execData.hNPP, lua_dll, MAX_PATH);
	while (nLen > 0 && lua_dll[nLen] != L'\\') lua_dll[nLen--] = 0;
	lstrcat(lua_dll, dll_name);
	WIN32_FIND_DATA f;
	return (FindFirstFile(lua_dll, &f) != INVALID_HANDLE_VALUE);
}

void InitFuncItem(int nItem, const TCHAR* szName, PFUNCPLUGINCMD pFunc, ShortcutKey* pShortcut)
{
	lstrcpy(funcItems[nItem]._itemName, szName);
	funcItems[nItem]._pFunc = pFunc;
	funcItems[nItem]._init2Check = false; //bCheck;
	funcItems[nItem]._pShKey = pShortcut;
}

HWND GetCurrentScintilla()
{
	int currentView = 0;
	SendNpp(NPPM_GETCURRENTSCINTILLA, 0, &currentView);
	return (currentView == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}

LRESULT SendSci(UINT iMessage, WPARAM wParam, LPARAM lParam)
{
    return SendMessage(GetCurrentScintilla(), iMessage, wParam, lParam);
}

void OnLove2D()
{
	TCHAR Love2D_path[MAX_PATH]{ 0 };
	lstrcpy(Love2D_path, g_opt.LovePath);
	
	//check love2d path
	WIN32_FIND_DATA f;
	HANDLE h = FindFirstFile(Love2D_path, &f);
	if (h == INVALID_HANDLE_VALUE) { 
		MessageBox(NULL, L"Love2D not found!", L"Error", MB_OK | MB_ICONERROR);
		OPENFILENAME ofn;
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.lpstrFilter = L"love.exe\0love.exe\0"; // filter
		ofn.lpstrTitle = L"Path to love.exe"; // title
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrInitialDir = L"%ProgramFiles%"; // initial dir
		ofn.lpstrFile = Love2D_path; // buffer for result
		if (!GetOpenFileName(&ofn)) return; // file not found or escape
		h = FindFirstFile(Love2D_path, &f);
		if(h == INVALID_HANDLE_VALUE) return; // file not accessible
		lstrcpy(g_opt.LovePath, Love2D_path); // save in options
	}

	TCHAR ws_full_path[MAX_PATH]{0};
	SendNpp(NPPM_GETCURRENTDIRECTORY, MAX_PATH, (LPARAM)ws_full_path);

	//check main.lua in current folder
	TCHAR main_path[MAX_PATH]{0};
	wsprintf(main_path, L"%s%s", ws_full_path, L"\\main.lua");
	h = FindFirstFile(main_path, &f);
	if (h == INVALID_HANDLE_VALUE) { MessageBox(NULL, L"File 'main.lua' not found!", L"", MB_OK | MB_ICONERROR); return; }

	TCHAR param_dir[MAX_PATH];
	wsprintf(param_dir, L"\"%s\"", ws_full_path);
	ShellExecute(NULL, L"open", Love2D_path, param_dir, NULL, SW_NORMAL);
}

void ResetItemCheck()
{
	for (auto i = (UINT8)LUA51; i <= (UINT8)LUAJIT; ++i)
		SendNpp(NPPM_SETMENUITEMCHECK, funcItems[i]._cmdID, g_opt.m_uInterpType == i);
	switch (g_opt.m_uInterpType)
	{
	case LUA54:
		wcscpy_s(ConsoleCaption, L" Console Lua54 ");
		break;

	case LUA53:
		wcscpy_s(ConsoleCaption, L" Console Lua53 ");
		break;

	case LUA52:
		wcscpy_s(ConsoleCaption, L" Console Lua52 ");
		break;

	case LUAJIT:
		wcscpy_s(ConsoleCaption, L" Console LuaJIT ");
		break;

	case LUA51:
	default:
		wcscpy_s(ConsoleCaption, L" Console Lua51 ");
	}

	SetWindowText(execData.hConsole, ConsoleCaption);
	if (g_opt.m_bConsoleOpenOnInit)
	if (is_floating < 4)
	{
		SetFocus(execData.hConsole);
		SetFocus(GetCurrentScintilla());
	}
	else
	{
		SendNpp(NPPM_DMMHIDE, 0, execData.hConsole);
		SendNpp(NPPM_DMMSHOW, 0, execData.hConsole);
	}
}

void OnLua51()
{
	g_opt.m_uInterpType = LUA51;
	ResetItemCheck();
	LM->reset_lib(path_lua51, LUA51);
}

void OnLua52()
{
	g_opt.m_uInterpType = LUA52;
	ResetItemCheck();
	LM->reset_lib(path_lua52, LUA52);
}

void OnLua53()
{
	g_opt.m_uInterpType = LUA53;
	ResetItemCheck();
	LM->reset_lib(path_lua53, LUA53);
}

void OnLua54()
{
	g_opt.m_uInterpType = LUA54;
	ResetItemCheck();
	LM->reset_lib(path_lua54, LUA54);
}

void OnLuaJIT()
{
	g_opt.m_uInterpType = LUAJIT;
	ResetItemCheck();
	LM->reset_lib(path_luajit, LUA51); // luajit based on Lua5.1
}

void GlobalInitialize()
{
	// Fetch the menu
	execData.hMenu = GetMenu(nppData._nppHandle);

	// Create the docking dialog
	execData.hConsole = CreateDialog(execData.hNPP,
		MAKEINTRESOURCE(IDD_CONSOLE), nppData._nppHandle,
		(DLGPROC)ConsoleProcDlg);

	dockingData.hClient = execData.hConsole;
	dockingData.pszName = ConsoleCaption;
	dockingData.dlgID = 0;
	dockingData.uMask = DWS_DF_CONT_BOTTOM;
	dockingData.pszModuleName = L"";

	// Register the docking dialog
	SendNpp(NPPM_DMMREGASDCKDLG, 0, &dockingData);
	SendNpp(NPPM_MODELESSDIALOG, execData.hConsole, MODELESSDIALOGADD);

	SendSci(SCI_SETMOUSEDWELLTIME, 500);

	g_opt.ReadOptions();

	g_opt.m_bConsoleOpenOnInit = !g_opt.m_bConsoleOpenOnInit;
	OpenCloseConsole();

	SendNpp(NPPM_SETMENUITEMCHECK, funcItems[AutoClear]._cmdID, g_opt.m_bConsoleAutoclear);
	SendNpp(NPPM_SETMENUITEMCHECK, funcItems[PrintTime]._cmdID, g_opt.m_bShowRunTime);

	bool fe51 = !file_exist(path_lua51);
	if (fe51) EnableMenuItem(execData.hMenu, funcItems[LUA51]._cmdID, MF_BYCOMMAND | MF_DISABLED);
	bool fe52 = !file_exist(path_lua52);
	if (fe52) EnableMenuItem(execData.hMenu, funcItems[LUA52]._cmdID, MF_BYCOMMAND | MF_DISABLED);
	bool fe53 = !file_exist(path_lua53);
	if (fe53) EnableMenuItem(execData.hMenu, funcItems[LUA53]._cmdID, MF_BYCOMMAND | MF_DISABLED);
	bool fe54 = !file_exist(path_lua54);
	if (fe54) EnableMenuItem(execData.hMenu, funcItems[LUA54]._cmdID, MF_BYCOMMAND | MF_DISABLED);
	bool fejit = !file_exist(path_luajit);
	if (fejit) EnableMenuItem(execData.hMenu, funcItems[LUAJIT]._cmdID, MF_BYCOMMAND | MF_DISABLED);
	if (fe51 && fe52 && fe53 && fe54 && fejit)
	{
		EnableMenuItem(execData.hMenu, funcItems[CheckSyntax]._cmdID, MF_BYCOMMAND | MF_DISABLED);
		EnableMenuItem(execData.hMenu, funcItems[RunScript]._cmdID, MF_BYCOMMAND | MF_DISABLED);
		EnableMenuItem(execData.hMenu, funcItems[CheckFiles]._cmdID, MF_BYCOMMAND | MF_DISABLED);
	}

	LM = new CLuaManager;
	switch (g_opt.m_uInterpType)
	{
	case LUA52:
		OnLua52();
		break;
	case LUA53:
		OnLua53();
		break;
	case LUA54:
		OnLua54();
		break;
	case LUAJIT:
		OnLuaJIT();
		break;
	case LUA51:
	default:
		OnLua51();
	}

	OriginalRichEditProc = (WNDPROC)SetWindowLongPtr(GetConsole(), GWLP_WNDPROC, (LONG_PTR)RichEditWndProc);
}

void GlobalDeinitialize()
{
	SendNpp(NPPM_MODELESSDIALOG, execData.hConsole,	MODELESSDIALOGREMOVE);
	g_opt.SaveOptions();
	HWND hDlgItem = GetConsole();
	if (hDlgItem) SetWindowLongPtr(hDlgItem, GWLP_WNDPROC, (LONG_PTR) OriginalRichEditProc);
	if (LM) delete LM;
}

void OnSwitchLang()
{
	g_opt.OnSwitchLang();
}

void OnShowAboutDlg()
{		
	TCHAR txt[] =
		L" Lua plugin v2.3 "
#ifdef _WIN64
		L"(64-bit)"
#else
		L"(32-bit)"
#endif  
		L"\n\n"\
		L" Author: Charsi <charsi2011@gmail.com>\n\n"\
		L" Syntax checking of Lua scripts.\n"\
		L" Run the script from current file.\n"\
		L" Run Love2D game from current directory.\n";
	MessageBox(NULL, txt, L"Lua plugin for Notepad++", MB_OK);
}

void OpenCloseConsole()
{
	g_opt.m_bConsoleOpenOnInit = !g_opt.m_bConsoleOpenOnInit;
	SendNpp(NPPM_SETMENUITEMCHECK, funcItems[ShowHideConsole]._cmdID, g_opt.m_bConsoleOpenOnInit);
	SendNpp(g_opt.m_bConsoleOpenOnInit ? NPPM_DMMSHOW : NPPM_DMMHIDE, 0, execData.hConsole);
}

HWND GetConsole()
{
	return GetDlgItem(execData.hConsole, IDC_RICHEDIT21);
}

void OnClearConsole()
{
	HWND hRE = GetConsole();
	int ndx = GetWindowTextLength(hRE);
	SendMessage(hRE, EM_SETSEL, 0, ndx);
	SendMessage(hRE, EM_REPLACESEL, 0, (LPARAM)L"");
	bNeedClear = false;
}

void OnSize()
{
	HWND hRE = GetConsole();
	if (hRE)
	{
		GetClientRect(execData.hConsole, &execData.rcConsole);
		SetWindowPos(hRE, NULL, 0, 0,
			execData.rcConsole.right, execData.rcConsole.bottom, SWP_NOZORDER);
	}
}

BOOL CALLBACK ConsoleProcDlg(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_NOTIFY:
		{
			NMHDR* pnmh = (NMHDR*) lParam;
			if (pnmh->hwndFrom == nppData._nppHandle)
			{
				switch( LOWORD(pnmh->code) )
				{
				case DMN_CLOSE: // closing dialog
					OpenCloseConsole();
					break;
				case DMN_FLOAT: // floating dialog
					is_floating = 8;
					break;
				case DMN_DOCK:  // docking dialog; HIWORD(pnmh->code) is dockstate [0..3]
					is_floating = HIWORD(pnmh->code);
					break;
				}
			}
		}
		break;		
		case WM_SIZE:
			OnSize();
		break;
    }
    return FALSE;
}

void AddStr(TCHAR* msg)
{
	if (!msg) return;
	HWND hRE = GetConsole();
	int ndx = GetWindowTextLength (hRE);
	SendMessage(hRE, EM_SETSEL, ndx, ndx);
	SendMessage(hRE, EM_REPLACESEL, 0, (LPARAM)msg);
}

void SetCharFormat(COLORREF color = RGB(0,0,0), DWORD dwMask = CFM_COLOR, DWORD dwEffects = 0, DWORD dwOptions = SCF_ALL)
{
	CHARFORMAT cf;
	cf.cbSize = sizeof(cf);
	cf.dwMask = dwMask;
	cf.dwEffects = dwEffects;
	cf.crTextColor = color; 
	SendMessage(GetConsole(), EM_SETCHARFORMAT, dwOptions, (LPARAM)&cf);
}

void OnPrintTime()
{
	g_opt.m_bShowRunTime = !g_opt.m_bShowRunTime;
	SendNpp(NPPM_SETMENUITEMCHECK, funcItems[PrintTime]._cmdID, g_opt.m_bShowRunTime);
}

void OnAutoClear()
{
	g_opt.m_bConsoleAutoclear = !g_opt.m_bConsoleAutoclear;
	SendNpp(NPPM_SETMENUITEMCHECK, funcItems[AutoClear]._cmdID, g_opt.m_bConsoleAutoclear);
}

bool not_valid_document(const TCHAR* ws_full_path)
{
	// save file
	SendNpp( NPPM_SAVECURRENTFILE, 0, 0 );
	if (ws_full_path[1]!=':' && ws_full_path[2]!='\\') return true;
	// get language type
	LangType docType = LangType::L_EXTERNAL;
	SendNpp( NPPM_GETCURRENTLANGTYPE, 0, (LPARAM)&docType );
	if (docType != LangType::L_LUA) return true; // не Lua скрипт!
	return false;
}

void AddMarker(int line)
{
	bool stat = line==0;
	SetCharFormat(RGB(stat ? 0 : 128, stat ? 128 : 0, 0));
	bNeedClear = true;
 	if (stat) return;
	line--;
	UINT start_sel = (UINT)SendSci(SCI_POSITIONFROMLINE, line);
	SendSci(SCI_GOTOPOS, start_sel); // ?
	SendSci(SCI_SETEMPTYSELECTION, start_sel);
	SendSci(SCI_MARKERDELETE, line, SC_MARK_ARROWS); // remove mark if added
	SendSci(SCI_MARKERADD, line, SC_MARK_ARROWS);
	SetFocus(GetCurrentScintilla());
}

void OnCheckSyntax()
{
	TCHAR ws_full_path[MAX_PATH]{ 0 };
	SendNpp(NPPM_GETFULLCURRENTPATH, MAX_PATH, (LPARAM)ws_full_path);
	if (not_valid_document(ws_full_path)) return;

	OnClearConsole();
	if (!g_opt.m_bConsoleOpenOnInit) OpenCloseConsole();

	char full_path[MAX_PATH];
	SysUniConv::UnicodeToMultiByte(full_path, MAX_PATH, ws_full_path);
	int line = LM->process(full_path);
	AddMarker(line);
}

void OnRunScript()
{
	TCHAR ws_full_path[MAX_PATH]{ 0 };
	SendNpp(NPPM_GETFULLCURRENTPATH, MAX_PATH, (LPARAM)ws_full_path);
	if (not_valid_document(ws_full_path)) return;

	if (g_opt.m_bConsoleAutoclear || bNeedClear) OnClearConsole();
	if (!g_opt.m_bConsoleOpenOnInit) OpenCloseConsole();

	char full_path[MAX_PATH]{0};
	SysUniConv::UnicodeToMultiByte(full_path, MAX_PATH, ws_full_path);

	clock_t run_time = clock();
	int rslt = LM->process(full_path, true);
	run_time = clock() - run_time;

	AddMarker(rslt);
	if(!rslt)
	{
		TCHAR str[MAX_PATH]{0};
		SYSTEMTIME sTime;
		GetLocalTime(&sTime);

		wsprintf(str, L"Success: %02d:%02d:%02d", sTime.wHour, sTime.wMinute, sTime.wSecond);
		AddStr(str);

		if (g_opt.m_bShowRunTime)
		{
			wsprintf(str, L"\r\nRuntime: %d ms", run_time);
			AddStr(str);
		}

		bNeedClear = false;
		SetCharFormat();
	}
}

void OnCheckFiles()
{
	TCHAR ws_full_path[MAX_PATH]{ 0 };
	SendNpp(NPPM_GETFULLCURRENTPATH, MAX_PATH, (LPARAM)ws_full_path); // fuul file path
	if (not_valid_document(ws_full_path)) return;

	//if (g_opt.m_bConsoleAutoclear)
	OnClearConsole();
	if (!g_opt.m_bConsoleOpenOnInit) OpenCloseConsole();

	TCHAR f_ext[MAX_PATH]{0};
	SendNpp( NPPM_GETEXTPART, MAX_PATH, (LPARAM)f_ext);
 	TCHAR ws_pattern[MAX_PATH]{0};
	SendNpp( NPPM_GETCURRENTDIRECTORY, MAX_PATH, (LPARAM)ws_full_path); // current dir
	
	wsprintf(ws_pattern, L"%s%s%s", ws_full_path, L"\\*", f_ext);
	WIN32_FIND_DATA f;
	HANDLE h = FindFirstFile(ws_pattern, &f);
	TCHAR ws_full_filepath[MAX_PATH]{0};
	int _cnt = 0;
	bool stat = true;
 	if (h != INVALID_HANDLE_VALUE)
 	{
 		do
 		{
			wsprintf(ws_full_filepath, L"%s%s%s", ws_full_path, L"\\", f.cFileName);
			char full_filepath[MAX_PATH]{ 0 };
			SysUniConv::UnicodeToMultiByte(full_filepath, MAX_PATH, ws_full_filepath);
 			bool isOK = !LM->process(full_filepath);
 			stat = stat && isOK;
 			if (!isOK) _cnt++;
 		} while (FindNextFile(h, &f));
 
 		wsprintf(ws_full_filepath, L"\r\nDone! Found %d file(s) with errors.", _cnt);
 		AddStr(ws_full_filepath);
 		SetCharFormat(RGB(stat ? 0 : 128, stat ? 128 : 0, 0));
 	}
}
#ifdef TEST_ITEM
void OnTestItem()
{
	OnClearConsole();
	AddStr(L"on test item clicked 2019");
}
#endif

///////////////////////////////////////////////////
// Main
BOOL APIENTRY
DllMain(HANDLE hModule, DWORD  reasonForCall, LPVOID lpReserved)
{
	switch (reasonForCall)
	{
	case DLL_PROCESS_ATTACH:
		execData.hNPP = (HINSTANCE)hModule;
		// init menu
		InitFuncItem(CheckSyntax,	loc_ru[CheckSyntax],	OnCheckSyntax);
		InitFuncItem(RunScript,		loc_ru[RunScript],		OnRunScript);
		InitFuncItem(CheckFiles,	loc_ru[CheckFiles],		OnCheckFiles);
		InitFuncItem(RunLove2D,		loc_ru[RunLove2D],		OnLove2D);
		//InitFuncItem(Separator1, L"");
		InitFuncItem(LUA51,			loc_ru[LUA51],			OnLua51);
		InitFuncItem(LUA52,			loc_ru[LUA52],			OnLua52);
		InitFuncItem(LUA53,			loc_ru[LUA53],			OnLua53);
		InitFuncItem(LUA54,			loc_ru[LUA54],			OnLua54);
		InitFuncItem(LUAJIT,		loc_ru[LUAJIT],			OnLuaJIT);
		//InitFuncItem(Separator2, L"");
		InitFuncItem(ShowHideConsole, loc_ru[ShowHideConsole], OpenCloseConsole);
		InitFuncItem(ClearConsole,	loc_ru[ClearConsole],	OnClearConsole);
		InitFuncItem(AutoClear,		loc_ru[AutoClear],		OnAutoClear);
		InitFuncItem(PrintTime,		loc_ru[PrintTime],		OnPrintTime);
		//InitFuncItem(Separator3, L"");
		InitFuncItem(SwitchLang,	loc_ru[SwitchLang],		OnSwitchLang);
		InitFuncItem(About,			loc_ru[About],			OnShowAboutDlg);
#ifdef TEST_ITEM
		InitFuncItem(TestItem,		loc_ru[TestItem], OnTestItem);
#endif
		m_hRichEditDll = LoadLibrary(L"Riched20.dll");
		break;

	case DLL_PROCESS_DETACH:
		if (m_hRichEditDll != nullptr)
			FreeLibrary(m_hRichEditDll);		
		break;
	}
	return TRUE;
}

extern "C" __declspec(dllexport) void setInfo(NppData notepadPlusData)
{
	nppData = notepadPlusData;
}

extern "C" __declspec(dllexport) const TCHAR * getName()
{
	return sPluginName;
}

extern "C" __declspec(dllexport) FuncItem * getFuncsArray(int *nbF)
{
	*nbF = nbFunc;
	return funcItems;
}

extern "C" __declspec(dllexport) void beNotified(SCNotification *notifyCode)
{
	if (notifyCode->nmhdr.hwndFrom == nppData._nppHandle)
		switch (notifyCode->nmhdr.code)
	{
		case NPPN_READY:
			GlobalInitialize();
			break;

		case NPPN_SHUTDOWN:
			GlobalDeinitialize();
			break;

		default:
			break;
	}
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT Message,
	WPARAM wParam, LPARAM lParam)
{
	return TRUE;
}

extern "C" __declspec(dllexport) BOOL isUnicode()
{
	return TRUE;
}