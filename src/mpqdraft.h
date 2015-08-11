#ifndef MPQDRAFT_H
#define MPQDRAFT_H

#include "MPQDraftPlugin.h"

class MPQDraftPluginInterface : public IMPQDraftPlugin
{
    HINSTANCE hInstance;
	public:
		BOOL WINAPI Identify(LPDWORD pluginID);
		BOOL WINAPI GetPluginName(LPSTR pPluginName,DWORD namebufferlength);
		BOOL WINAPI CanPatchExecutable(LPCSTR exefilename);
		BOOL WINAPI Configure(HWND parentwindow);
		BOOL WINAPI ReadyForPatch();
		BOOL WINAPI GetModules(MPQDRAFTPLUGINMODULE* pluginmodules,LPDWORD nummodules);
		BOOL WINAPI InitializePlugin(IMPQDraftServer* server);
		BOOL WINAPI TerminatePlugin();
		void SetInstance(HINSTANCE hInst);
};



#endif // MPQDRAFT_H

