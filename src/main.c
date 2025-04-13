#include <windows.h>
#include <stdio.h>
#include "patcher.h" // Your custom patcher interface
#include "msg.h"     // For msg_err_a or your logging macro
#include <game.h>
#include <conio.h>

BOOL OpenFileDialogA(char* filePath, DWORD bufferSize)
{
    OPENFILENAMEA ofn = {0};
    ZeroMemory(filePath, bufferSize);

    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.lpstrFilter = "Executable Files\0*.exe\0All Files\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = bufferSize;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = "Select an Executable";

    return GetOpenFileNameA(&ofn);
}

int __internal_patch()
{
#ifdef TEST_BUILD
    printf("Running for test binary\n");
#endif
    EnableDebugPrivilege();

    char exePath[MAX_PATH];
    char workingDirectory[MAX_PATH];
    if (!OpenFileDialogA(exePath, sizeof(exePath)))
    {
        printf("No file selected.\n");
        return 1;
    }
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);

    char* lastSlash = strrchr(exePath, '\\');
    if (lastSlash != NULL)
    {
        size_t length = lastSlash - exePath;
        strncpy(workingDirectory, exePath, length);
        workingDirectory[length] = '\0';
    }
    else
    {
        // If no backslash, assume the executable is in the current directory
        GetCurrentDirectoryA(MAX_PATH, workingDirectory);
    }

    printf("Starting %s in\n ->%s\n", exePath, workingDirectory);

    BOOL created = CreateProcessA(exePath, "-bylauncher", NULL, NULL, FALSE, CREATE_SUSPENDED | DETACHED_PROCESS, NULL,
                                  workingDirectory, &si, &pi);

    if (!created)
    {
        printf("CreateProcess failed. Error: %lu\n", GetLastError());
        return 1;
    }

    printf("Process created in suspended state. PID: %lu\n", pi.dwProcessId);

#ifndef TEST_BUILD
    const DWORD baseOffset = 0xD9D5FF;
    BYTE patchBytes[] = {0xc7, 0x03, 0x0, 0x0, 0x0, 0x0};
#else
    const DWORD baseOffset = 0x16d8;
    BYTE patchBytes[] = {0x74, 0x26};
#endif

    Instruction newInstruction = {patchBytes, sizeof(patchBytes)};

    DWORD pid = pi.dwProcessId;

    printf("Found game handle @ %ld\n", pid);

    // 3. Patch the process using the handle from CreateProcessW
    if (ReplaceInstructionInProcess(pid, baseOffset, &newInstruction))
    {
        msg_info_a("Instruction replacement successful.\n");
    }
    else
    {
        msg_err_a("Instruction replacement failed.\n");
        return 1;
    }

    DWORD resumeResult = ResumeThread(pi.hThread);
    printf("Resuming process %ld\n", resumeResult);
    if (resumeResult == (DWORD)-1)
    {
        printf("Failed to resume the process. Error: %lu\n", GetLastError());
        return 1;
    }

    printf("Process resumed successfully.\n");

    CloseHandle(pi.hThread);

    DWORD waitResult = WaitForSingleObject(pi.hProcess, INFINITE);
    if (waitResult == WAIT_FAILED)
    {
        printf("Failed to wait for process. Error: %lu\n", GetLastError());
        return 1;
    }

    printf("Child process finished.\n");

    // Close the process handle
    CloseHandle(pi.hProcess);

    return 0;
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