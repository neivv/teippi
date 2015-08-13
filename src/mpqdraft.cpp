#include "mpqdraft.h"

#include <windows.h>
#include <stdint.h>

#include "mainpatch.h"

#ifdef DEBUG
#define PLUGIN_NAME "Limits v 1.0 (Debug)"
#define PLUGIN_ID 0x4230daec
#else
#define PLUGIN_NAME "Limits v 1.0"
#define PLUGIN_ID 0x4230deac
#endif

MPQDraftPluginInterface thePluginInterface;

#ifdef __GNUC__
extern "C" void Initialize() __attribute__ ((visibility ("default")));
extern "C" bool Metaplugin_Init() __attribute__ ((visibility ("default")));
extern "C" BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD ul_reason_for_call, LPVOID lpReserved) __attribute__ ((visibility ("default")));
extern "C" BOOL __stdcall GetMPQDraftPlugin(IMPQDraftPlugin **lppMPQDraftPlugin) __attribute__ ((visibility ("default")));
#endif

BOOL WINAPI MPQDraftPluginInterface::Identify(LPDWORD pluginID)
{
    *pluginID = PLUGIN_ID;
    return TRUE;
}

BOOL WINAPI MPQDraftPluginInterface::GetPluginName(LPSTR pPluginName, DWORD namebufferlength)
{
    strncpy(pPluginName, PLUGIN_NAME, namebufferlength);
    return TRUE;
}

BOOL WINAPI MPQDraftPluginInterface::CanPatchExecutable(LPCSTR exefilename)
{
    return TRUE;
}

BOOL WINAPI MPQDraftPluginInterface::Configure(HWND parentwindow)
{
    return TRUE;
}

BOOL WINAPI MPQDraftPluginInterface::ReadyForPatch()
{
    return TRUE;
}

BOOL WINAPI MPQDraftPluginInterface::GetModules(MPQDRAFTPLUGINMODULE* pluginmodules, LPDWORD nummodules)
{
    *nummodules = 0;
    return TRUE;
}

extern "C" void Initialize()
{
    #ifdef DEBUG
    unsigned long justQuit = GetTickCount() + 1000 * 60;
    if(GetKeyState(VK_SCROLL) & 1)
    {
        while(!IsDebuggerPresent())
        {
            if ((justQuit < GetTickCount()) || !(GetKeyState(VK_SCROLL) & 1))
                exit(42);
            Sleep(1234);
        }
        //asm("int3");
    }
    #endif
    InitialPatch();
}

BOOL WINAPI MPQDraftPluginInterface::InitializePlugin(IMPQDraftServer* server)
{
    Initialize();
    return TRUE;
}

extern "C" bool Metaplugin_Init()
{
    Initialize();
    return true;
}

BOOL WINAPI MPQDraftPluginInterface::TerminatePlugin()
{
	// Does not get ever called
    return TRUE;
}

void MPQDraftPluginInterface::SetInstance(HINSTANCE hInst)
{
    hInstance = hInst;
}

extern "C" BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
    {
		case DLL_PROCESS_ATTACH:
			DisableThreadLibraryCalls(hInstance);
			thePluginInterface.SetInstance(hInstance);
			break;
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		    break;
		case DLL_PROCESS_DETACH:
			break;
	}
	return TRUE;
}

extern "C" BOOL __stdcall GetMPQDraftPlugin(IMPQDraftPlugin **lppMPQDraftPlugin)
{
	*lppMPQDraftPlugin = &thePluginInterface;
	return TRUE;
}
