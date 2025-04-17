#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <patcher.h>
#include <string.h>
#include <tlhelp32.h>

BOOL SelectExecutable(char* filePath, DWORD bufferSize)
{
    OPENFILENAMEA ofn = {0};
    ZeroMemory(filePath, bufferSize);

    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = bufferSize;
    ofn.lpstrTitle = "Select Game Executable";
    ofn.lpstrFilter = "Executable Files\0*.exe\0All Files\0*.*\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    return GetOpenFileNameA(&ofn);
}

int main()
{
    EnableDebugPrivilege();
    BYTE patchBytes[] = {0xc7, 0x03, 0x0, 0x0, 0x0, 0x0};
    Instruction patch = {patchBytes, sizeof(patchBytes)};

    const DWORD PATCH_OFFSET = 0xD9D5FF;
    char exePath[MAX_PATH];
    if (!SelectExecutable(exePath, sizeof(exePath)))
    {
        printf("No file selected.\n");
        return 1;
    }

    char workingDir[MAX_PATH];
    strcpy(workingDir, exePath);
    char* lastSlash = strrchr(workingDir, '\\');
    if (lastSlash)
    {
        *lastSlash = '\0';
    }
    else
    {
        GetCurrentDirectoryA(MAX_PATH, workingDir);
    }

    printf("Launching: %s\n", exePath);

    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);

    char cmdLine[1024];
    snprintf(cmdLine, sizeof(cmdLine), "\"%s\" -bylauncher", exePath);

    BOOL result = CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE, CREATE_SUSPENDED | CREATE_NEW_CONSOLE, NULL,
                                 workingDir, &si, &pi);

    if (!result)
    {
        printf("Failed to start process. Error: %lu\n", GetLastError());
        return 1;
    }

    printf("Game process created in suspended state (PID: %lu)\n", pi.dwProcessId);

    if (!ReplaceInstructionInProcess(pi.dwProcessId, PATCH_OFFSET, &patch))
    {
        printf("Patching failed.\n");
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    ResumeThread(pi.hThread);
    printf("Process resumed. Awaiting game exit...\n");

    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    printf("Game exited. Press any key to quit...\n");
    getchar();
    return 0;
}