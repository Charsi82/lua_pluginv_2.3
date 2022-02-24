#pragma once

#include "helpers.h"
#define OptSectName L"Options"
#define OptFlagsKey L"Flags"
#define OptLangKey  L"Language"
#define OptLuaKey  L"Lua"
#define OptLovePath L"LovePath"

class CPluginOptions
{
    public:
        CPluginOptions();

		bool m_bConsoleOpenOnInit; //открывать при запуске
		bool m_bConsoleClearOnRun;
		bool m_bConsoleAutoclear;
		bool m_bShowRunTime;
		BYTE  m_uInterpType; // Lua interpretator type

        void ReadOptions();
        void SaveOptions();
		bool MustBeSaved() const;

		void OnSwitchLang();
		void ApplyLang();
        BYTE m_uFlags;
		TCHAR LovePath[MAX_PATH];

	private:
        BYTE getOptFlags() const;
		TCHAR szIniFilePath[MAX_PATH];

        enum eOptConsts {		   
			   OPTF_CONOPENONINIT = 0x01,
			   OPTF_CONCLEARONRUN = 0x02,
			   OPTF_CONAUTOCLEAR  = 0x04,
			   OPTF_PRINTRUNTIME  = 0x08
        };
		BYTE  m_uFlags0;
		BYTE  m_uLang;
		BYTE  m_uLang0;
		BYTE  m_uInterpType0;
};
