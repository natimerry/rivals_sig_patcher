#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <tlhelp32.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

bool InjectDLL(DWORD pid, const char* dllPath)
{
    HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                                      PROCESS_VM_WRITE | PROCESS_VM_READ,
                                  FALSE, pid);
    if (!hProcess)
    {
        printf("[-] Failed to open process (PID: %lu)\n", pid);
        return false;
    }

    size_t pathLen = strlen(dllPath) + 1;
    LPVOID remoteMem = VirtualAllocEx(hProcess, NULL, pathLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMem)
    {
        printf("[-] VirtualAllocEx failed: %lu\n", GetLastError());
        CloseHandle(hProcess);
        return false;
    }

    if (!WriteProcessMemory(hProcess, remoteMem, dllPath, pathLen, NULL))
    {
        printf("[-] WriteProcessMemory failed: %lu\n", GetLastError());
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    LPTHREAD_START_ROUTINE loadLibAddr =
        (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    if (!loadLibAddr)
    {
        printf("[-] Failed to get LoadLibraryA address\n");
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, loadLibAddr, remoteMem, 0, NULL);
    if (!hThread)
    {
        printf("[-] CreateRemoteThread failed: %lu\n", GetLastError());
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    CloseHandle(hThread);
    CloseHandle(hProcess);
    return true;
}

bool ReadConfig(char* exePath, char* dllPath)
{
    FILE* config = fopen("config.ini", "r");
    if (!config)
    {
        printf("[-] Failed to open config.ini\n");
        return false;
    }

    fgets(exePath, MAX_PATH, config);
    exePath[strcspn(exePath, "\r\n")] = 0; // Strip newline

    fgets(dllPath, MAX_PATH, config);
    dllPath[strcspn(dllPath, "\r\n")] = 0;

    fclose(config);
    return true;
}

void GetDirectoryFromPath(const char* fullPath, char* outDir)
{
    const char* lastSlash = strrchr(fullPath, '\\');
    if (!lastSlash)
        lastSlash = strrchr(fullPath, '/');

    if (lastSlash)
    {
        size_t len = lastSlash - fullPath;
        strncpy(outDir, fullPath, len);
        outDir[len] = '\0';
    }
    else
    {
        // fallback to current dir if path is malformed
        strcpy(outDir, ".");
    }
}

int main()
{
    char exePath[MAX_PATH], dllPath[MAX_PATH];
    if (!ReadConfig(exePath, dllPath))
    {
        printf("Failed to read config\n");
        return 1;
    }

    char cmdLine[1024];
    snprintf(cmdLine, sizeof(cmdLine), "\"%s\" -bylauncher", exePath);

    char workingDir[MAX_PATH];
    GetDirectoryFromPath(exePath, workingDir);

    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;

    // Create process in suspended state
    if (!CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, workingDir, &si, &pi))
    {
        printf("[-] Failed to launch game: %lu\n", GetLastError());
        return 1;
    }

    printf("[+] Game launched in suspended state. PID: %lu\n", pi.dwProcessId);

    // Inject the DLL into the suspended process
    if (!InjectDLL(pi.dwProcessId, dllPath))
    {
        printf("[-] DLL injection failed.\n");
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    printf("[+] DLL injected successfully.\n");

    // Resume the process after DLL injection
    if (ResumeThread(pi.hThread) == (DWORD)-1)
    {
        printf("[-] Failed to resume the process: %lu\n", GetLastError());
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    printf("[+] Process resumed.\n");

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    printf("Press any key to exit...\n");
    getchar();
    return 0;
}
