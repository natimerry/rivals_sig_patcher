#include <msg.h>
#include <windows.h>
#include <patcher.h>
#include <tlhelp32.h>
#include <psapi.h> // For GetModuleInformation
#include <stdio.h>
#include <game.h>
#include <stdbool.h>
void PrintLastError(const char* msg)
{
    DWORD err = GetLastError();
    char* errMsg = NULL;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                   err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&errMsg, 0, NULL);
    printf("%s (Error %lu): %s", msg, err, errMsg);
    LocalFree(errMsg);
}

BOOL EnableDebugPrivilege()
{
    HANDLE hToken = NULL;
    LUID luid = {};
    TOKEN_PRIVILEGES tp = {};

    // Open the current process token
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    {
        wprintf(L"OpenProcessToken failed. Error: %lu\n", GetLastError());
        return FALSE;
    }

    // Lookup the LUID for SeDebugPrivilege
    if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid))
    {
        wprintf(L"LookupPrivilegeValue failed. Error: %lu\n", GetLastError());
        CloseHandle(hToken);
        return FALSE;
    }

    // Set up the TOKEN_PRIVILEGES structure
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL))
    {
        wprintf(L"AdjustTokenPrivileges failed. Error: %lu\n", GetLastError());
        CloseHandle(hToken);
        return FALSE;
    }

    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)
    {
        wprintf(L"SeDebugPrivilege not assigned. You may lack permissions.\n");
        CloseHandle(hToken);
        return FALSE;
    }

    CloseHandle(hToken);
    wprintf(L"Debug privelages acquired");
    return TRUE;
}

BOOL ReplaceInstructionInProcess(DWORD processId, DWORD baseOffset, const Instruction* newInstruction)
{
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (hProcess == NULL)
    {
        PrintLastError("OpenProcess failed");
        return FALSE;
    }

    HMODULE moduleBaseAddress = GetModuleBaseAddress(processId);
    if (moduleBaseAddress == NULL)
    {
        PrintLastError("GetModuleBaseAddress failed");
        CloseHandle(hProcess);
        return FALSE;
    }

    LPVOID targetAddress = (LPVOID)((uintptr_t)moduleBaseAddress + baseOffset);
    SIZE_T bytesToWrite = newInstruction->size;
    SIZE_T bytesWritten = 0;

    if (!WriteProcessMemory(hProcess, targetAddress, newInstruction->data, bytesToWrite, &bytesWritten))
    {
        PrintLastError("WriteProcessMemory failed");
        CloseHandle(hProcess);
        return FALSE;
    }

    if (!FlushInstructionCache(hProcess, targetAddress, bytesToWrite))
    {
        PrintLastError("FlushInstructionCache failed");
    }

    printf("Successfully replaced %zu bytes at address %p (module base + 0x%lX) in process %lu.\n", bytesWritten,
           targetAddress, baseOffset, processId);

    CloseHandle(hProcess);
    return TRUE;
}
