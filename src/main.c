#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <tlhelp32.h>
#include <patcher.h>
#include <string.h>

#define TARGET_PROCESS_NAME "Marvel-Win64-Shipping.exe" // <- Set actual name here
#define PATCH_OFFSET        0xD9D5FF

DWORD FindProcessId(const char* processName)
{
    PROCESSENTRY32 pe32;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;

    pe32.dwSize = sizeof(PROCESSENTRY32);
    if (!Process32First(snapshot, &pe32))
    {
        CloseHandle(snapshot);
        return 0;
    }

    do
    {
        if (_stricmp(pe32.szExeFile, processName) == 0)
        {
            CloseHandle(snapshot);
            return pe32.th32ProcessID;
        }
    } while (Process32Next(snapshot, &pe32));

    CloseHandle(snapshot);
    return 0;
}

void SuspendAllThreads(DWORD pid)
{
    THREADENTRY32 te32 = {.dwSize = sizeof(THREADENTRY32)};
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return;

    if (Thread32First(snapshot, &te32))
    {
        do
        {
            if (te32.th32OwnerProcessID == pid)
            {
                HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te32.th32ThreadID);
                if (hThread)
                {
                    SuspendThread(hThread);
                    CloseHandle(hThread);
                }
            }
        } while (Thread32Next(snapshot, &te32));
    }

    CloseHandle(snapshot);
}

void ResumeAllThreads(DWORD pid)
{
    THREADENTRY32 te32 = {.dwSize = sizeof(THREADENTRY32)};
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return;

    if (Thread32First(snapshot, &te32))
    {
        do
        {
            if (te32.th32OwnerProcessID == pid)
            {
                HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te32.th32ThreadID);
                if (hThread)
                {
                    while (ResumeThread(hThread) > 0)
                        ;
                    CloseHandle(hThread);
                }
            }
        } while (Thread32Next(snapshot, &te32));
    }

    CloseHandle(snapshot);
}

int main()
{
    EnableDebugPrivilege();

    BYTE patchBytes[] = {0xc7, 0x03, 0x0, 0x0, 0x0, 0x0};
    Instruction patch = {patchBytes, sizeof(patchBytes)};

    printf("Waiting for %s to start...\n", TARGET_PROCESS_NAME);

    DWORD pid = 0;
    while ((pid = FindProcessId(TARGET_PROCESS_NAME)) == 0)
    {
        Sleep(1);
    }

    printf("Found process (PID: %lu). Suspending threads...\n", pid);
    SuspendAllThreads(pid);

    if (!ReplaceInstructionInProcess(pid, PATCH_OFFSET, &patch))
    {
        printf("Patching failed. Exiting...\n");
        return 1;
    }

    printf("Patch applied. Resuming process...\n");
    ResumeAllThreads(pid);

    HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (hProcess)
    {
        printf("Waiting for game to exit...\n");
        WaitForSingleObject(hProcess, INFINITE);
        CloseHandle(hProcess);
    }

    printf("Game exited. Press any key to quit...\n");
    getchar();
    return 0;
}
