#include <windows.h>
#include <stdio.h>
#include "patcher.h" // Your custom patcher interface
#include "msg.h"     // For msg_err_a or your logging macro
#include <game.h>
#include <conio.h>
int __internal_patch()
{
    const DWORD baseOffset = 0xD9D5FF;
    BYTE patchBytes[] = {0xc7, 0x03, 0x0};

    Instruction newInstruction = {patchBytes, sizeof(patchBytes)};

    DWORD pid = 0;

    printf("Waiting for game handle...\n");

    while (pid == 0)
    {
#ifndef PATCHER_DLL
        if (_kbhit())
        {
            int ch = _getwch();
            if (ch == 27) // ESC key
            {
                printf("Manual exit.\n");
                exit(0);
            }
        }
#endif
        pid = get_game_handle();
        Sleep(100);
    }

    printf("Found game handle @ %ld\n", pid);

    // 3. Patch the process using the handle from CreateProcessW
    if (ReplaceInstructionInProcess(pid, baseOffset, &newInstruction))
    {
        msg_info_a("Instruction replacement successful.\n");
        return 0;
    }
    msg_err_a("Instruction replacement failed.\n");
    return 1;
}
#ifdef PATCHER_DLL
__declspec(dllexport) int PatchMarvelRivals()
{
    exit(__internal_patch());
}
#else

int main()
{
    SetConsoleCtrlHandler(NULL, FALSE);
    exit(__internal_patch());
}
#endif