#include "lua_main.h"
#include <stdio.h>

#define LUA_GLOBALSINDEX	(-10002)	//Lua5.1
#define LUAREG(L, name, func) (lua_pushcclosure(L, (func), 0), m_lua_setglobal(L, (name)))
#define LOAD_FUNCTION(tp, fn) fn = (tp)GetProcAddress(hinstLib, #fn); if (!fn) AddStr(L"'"#fn"' not loaded\r\n");
#define lua_pop(L,n) lua_settop(L, -(n)-1)

extern CLuaManager* LM;
void SetCharFormat(COLORREF color = RGB(0, 0, 0), DWORD dwMask = CFM_COLOR, DWORD dwEffects = 0, DWORD dwOptions = SCF_ALL);

void print_from_lua(const char* txt)
{
	int iSize = strlen(txt);
	TCHAR* wtmp = new TCHAR[iSize + 1]{ 0 };
	SysUniConv::MultiByteToUnicode(wtmp, iSize, txt, iSize);
	AddStr(wtmp);
	delete[] wtmp;
}

int luafunc_msgbox(void* L)
{
	LM->msgbox();
	return 1;
}

void CLuaManager::msgbox()
{
	int res = 0;
	switch (lua_gettop(L))
	{
	case 1:
		res = MessageBoxA(NULL, lua_tolstring(L, 1, 0), "", MB_OK);
		break;
	case 2:
		res = MessageBoxA(NULL, lua_tolstring(L, 1, 0), lua_tolstring(L, 2, 0), MB_OK);
		break;
	default:
		const char* icon = lua_tolstring(L, 3, 0);
		long icon_type = MB_OK;
		if (strstr(icon, "error")) icon_type |= MB_ICONERROR;
		if (strstr(icon, "warning")) icon_type |= MB_ICONWARNING;
		if (strstr(icon, "info")) icon_type |= MB_ICONINFORMATION;
		if (strstr(icon, "question")) icon_type |= MB_ICONQUESTION;
		if (strstr(icon, "yesnocancel")) icon_type |= MB_YESNOCANCEL;
		else if (strstr(icon, "yesno")) icon_type |= MB_YESNO;
		res = MessageBoxA(NULL, lua_tolstring(L, 1, 0), lua_tolstring(L, 2, 0), icon_type);
	}
	lua_pushinteger(L, res);
}

int luafunc_luahelp(void* L)
{
	print_from_lua(
"function show messagebox window\r\n\
arg1: text massage\r\n\
arg2 : text title\r\n\
arg3 : text from { 'ok', 'error' , 'warning', 'info', 'yesnocancel', 'yesno' }\r\n\
return : code button\r\n\
msgbox('msg' [[, 'title'], icon_type] )\r\n\
\r\n\
function return table with file names by mask\r\n\
list_files('.\\\\*.*')\r\n\
\r\n\
function print this help\r\n\
lua_help()\r\n"
	);
	return 0;
}

int luafunc_list_files(void* L)
{
	LM->list_files();
	return 1;
}

void CLuaManager::list_files()
{
	WIN32_FIND_DATAA f;
	const char* mask = lua_tolstring(L, -1, 0);
	HANDLE h = FindFirstFileA(mask, &f);
	int i = 1;
	lua_createtable(L, 0, 0);
	if (h != INVALID_HANDLE_VALUE)
	do {
		lua_pushstring(L, f.cFileName);
		lua_rawseti(L, -2, i++);
	} while (FindNextFileA(h, &f));
}

int luafunc_print(void* L)
{
	LM->print();
	return 0;
}

void CLuaManager::print()
{
	int sz = lua_gettop(L);
	if (!sz) return;
	m_lua_getglobal(L, "tostring");
	for (int i = 1; i <= sz; i++)
	{
		lua_pushvalue(L, -1);
		lua_pushvalue(L, i);
		m_lua_pcall(L, 1, 1);
		const char* str = lua_tolstring(L, -1, 0);
		if (i>1) print_from_lua(" ");
		if (str)
			print_from_lua(str);
		else {
			print_from_lua(lua_typename(L, lua_type(L, i)));
		}
		lua_pop(L, 1);
	}
	lua_settop(L, 0);
	print_from_lua("\r\n");
}

void CLuaManager::reset_lib(const TCHAR* dll_name, BYTE lib_type)
{
	intrp_type = lib_type;
	if (hinstLib)
	{
		if (L) lua_close(L);
		L = NULL;
		FreeLibrary(hinstLib);
	}

	TCHAR lua_dll[MAX_PATH]{ 0 };
	UINT nLen = GetModuleFileName((HMODULE)execData.hNPP, lua_dll, MAX_PATH);
	while (nLen > 0 && lua_dll[nLen] != L'\\') lua_dll[nLen--] = 0;
	lstrcat(lua_dll, dll_name);

	hinstLib = LoadLibrary(lua_dll);
	if (hinstLib)
	{
		SetCharFormat();
		OnClearConsole();

		LOAD_FUNCTION(voidfunc*, luaL_newstate);
		LOAD_FUNCTION(varfuncvoid*, luaL_openlibs);
		LOAD_FUNCTION(varfuncvoid*, lua_close);
		LOAD_FUNCTION(varfuncchar*, lua_pushstring);
		LOAD_FUNCTION(varfuncvoid*, lua_pushvalue);
		LOAD_FUNCTION(varfuncvoid*, lua_pushinteger);
		LOAD_FUNCTION(varfuncvoid*, lua_pushcclosure);
		LOAD_FUNCTION(varfuncchar*, lua_tolstring);
		LOAD_FUNCTION(varfuncvoid*, lua_setfield);
		LOAD_FUNCTION(varfuncvoid*, lua_getfield);
		LOAD_FUNCTION(varfuncvoid*, lua_settop);
		LOAD_FUNCTION(varfuncint*,  lua_gettop);
		LOAD_FUNCTION(varfuncint*,  lua_type);
		LOAD_FUNCTION(varfuncchar*, lua_typename);
		LOAD_FUNCTION(varfuncvoid*, lua_createtable);
		LOAD_FUNCTION(varfuncvoid*, lua_rawseti);

		if (intrp_type == LUA51)
		{
			LOAD_FUNCTION(varfuncvoid*, lua_call);
			LOAD_FUNCTION(varfuncint*, lua_pcall);
			LOAD_FUNCTION(varfuncint*, lua_tointeger);
		}
		else
		{
			LOAD_FUNCTION(varfuncvoid*, lua_callk);
			LOAD_FUNCTION(varfuncint*, lua_pcallk);
			LOAD_FUNCTION(varfuncint*, lua_tointegerx);
			LOAD_FUNCTION(varfuncvoid*, lua_setglobal);
			LOAD_FUNCTION(varfuncvoid*, lua_getglobal);
		}

		L = luaL_newstate();
		luaL_openlibs(L);

		//init_lua_functions
		LUAREG(L, "msgbox", luafunc_msgbox);
		LUAREG(L, "print", luafunc_print);
		LUAREG(L, "list_files", luafunc_list_files);
		LUAREG(L, "lua_help", luafunc_luahelp);
	}
	else
	{
		TCHAR buf[MAX_PATH]{ 0 };
		wsprintf(buf, L"Can't load '%s'", lua_dll);
		MessageBox(NULL, buf, L"Error", MB_OK | MB_ICONERROR);
	}
}

CLuaManager::~CLuaManager()
{
	if (L) lua_close(L);
	if (hinstLib) FreeLibrary(hinstLib);
}

void CLuaManager::m_lua_call(void* L, int narg, int nret)
{
	if (lua_type(L, -narg - 1) != 6 /*LUA_TFUNCTION*/) {
		MessageBox(NULL, L"try to call non function object", L"Error", MB_OK);
		return;
	}
	if (intrp_type == LUA51)
		lua_call(L, narg, nret);
	else
		lua_callk(L, narg, nret, 0, NULL);
}

int CLuaManager::m_lua_pcall(void* L, int narg, int nret)
{
	if (lua_type(L, -narg - 1) != 6 /*LUA_TFUNCTION*/) {
		MessageBox(NULL, L"try to call non function object", L"Error", MB_OK);
		return 0;
	}
	if (intrp_type == LUA51)
		return lua_pcall(L, narg, nret, 0);
	else
		return lua_pcallk(L, narg, nret, 0, 0, NULL);
}

void CLuaManager::m_lua_getglobal(void* _L, const char* name)
{
	if (intrp_type == LUA51)
		lua_getfield(L, LUA_GLOBALSINDEX, name);
	else
		lua_getglobal(L, name);
}

void CLuaManager::m_lua_setglobal(void* _L, const char* name)
{
	if (intrp_type == LUA51)
		lua_setfield(L, LUA_GLOBALSINDEX, name);
	else
		lua_setglobal(L, name);
}

int CLuaManager::m_lua_tointeger(void* _L, int idx)
{
	if (intrp_type == LUA51)
		return lua_tointeger(L, idx);
	else
		return lua_tointegerx(L, idx, 0);
}

int CLuaManager::process(const char* fpath, bool run)
{
	if (!L) {
		AddStr(L"Error:Lua not initialized!");
		return -1;
	}
	char lua_code[1024]{ 0 }; // todo with pushvfstring
	if (run)
		sprintf(lua_code, "local res,err = pcall(dofile,[[%s]]) if res then return 0 end print(err) return tonumber(err:match(':(%s):')) or -1", fpath, "%d+");
	else
		sprintf(lua_code, "local res,err = loadfile([[%s]]) if res then print([[%s - syntax OK]]) return 0 end print(err) return tonumber(err:match(':(%s):')) or -1", fpath, fpath, "%d+");
	m_lua_getglobal(L, (intrp_type == LUA51) ? "loadstring" :"load");
	lua_pushstring(L, lua_code);
	m_lua_call(L, 1, 1); // OK -> function + nil, not OK -> nil + errstring
	m_lua_pcall(L, 0, 1); // call created function (0 - no arguments, 1 - results)
	int res = m_lua_tointeger(L, -1);
	lua_settop(L, 0);
	return res;
}

/*void CLuaManager::dump_stack()
{
	if (!L) return;
	print_from_lua("\r\n==== dump start ====");
	int sz = lua_gettop(L);
	m_lua_getglobal(L, "tostring");
	for (int i = 1; i <= sz; i++)
	{
		lua_pushvalue(L, -1);
		lua_pushvalue(L, i);
		m_lua_pcall(L, 1, 1);
		const char* str = lua_tolstring(L, -1, 0);
		print_from_lua("\r\n");
		if (str)
			print_from_lua(str);
		else {
			print_from_lua(lua_typename(L, lua_type(L, i)));
		}
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
	print_from_lua("\r\n===== dump end ====\r\n");
}*/