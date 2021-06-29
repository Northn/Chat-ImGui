#include "pch.h"
#include "ChatImGui.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
        gChat.initialize();
    else if (fdwReason == DLL_PROCESS_DETACH)
    {
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    return TRUE;
}
