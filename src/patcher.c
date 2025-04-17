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
    wprintf(L"Debug privelages acquired\n");
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

    HMODULE moduleBaseAddress = NULL;
    if (hProcess)
    {
        moduleBaseAddress = GetImageBaseNtQuery(hProcess);
    }
    if (moduleBaseAddress == NULL)
    {
        PrintLastError("GetImageBaseNtQuery failed");
        CloseHandle(hProcess);
        return FALSE;
    }

    LPVOID targetAddress = (LPVOID)((uintptr_t)moduleBaseAddress + baseOffset);
    SIZE_T bytesToWrite = newInstruction->size;
    SIZE_T bytesWritten = 0;
    DWORD oldProtect = 0;

    MEMORY_BASIC_INFORMATION mbi = {0};

    // Change memory protection to allow writing
    if (!VirtualProtectEx(hProcess, targetAddress, bytesToWrite, PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        PrintLastError("VirtualProtectEx failed");
        CloseHandle(hProcess);
        return FALSE;
    }

    // Read updated memory addresses
    if (VirtualQueryEx(hProcess, targetAddress, &mbi, sizeof(mbi)))
    {
        printf("BaseAddress: 0x%p\n", mbi.BaseAddress);
        printf("RegionSize:  0x%llx\n", (DWORD_PTR)mbi.RegionSize);
        printf("State:       0x%lx\n", mbi.State);
        printf("Protect:     0x%lx\n", mbi.Protect);
        printf("Type:        0x%lx\n", mbi.Type);
    }
    else
    {
        PrintLastError("VirtualQueryEx failed");
    }

    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll)
    {
        PrintLastError("GetModuleHandleA(nntdll.dll) failed");
        CloseHandle(hProcess);
        return FALSE;
    }

    typedef NTSTATUS(NTAPI * NtWriteVirtualMemory_t)(HANDLE ProcessHandle, PVOID BaseAddress, PVOID Buffer,
                                                     ULONG NumberOfBytesToWrite, PULONG NumberOfBytesWritten);

    NtWriteVirtualMemory_t NtWriteVirtualMemory =
        (NtWriteVirtualMemory_t)GetProcAddress(hNtdll, "NtWriteVirtualMemory");

    if (!NtWriteVirtualMemory)
    {
        PrintLastError("GetProcAddress(NtWriteVirtualMemory) failed");
        CloseHandle(hProcess);
        return FALSE;
    }

    ULONG ntBytesWritten = 0;
    NTSTATUS status =
        NtWriteVirtualMemory(hProcess, targetAddress, newInstruction->data, (ULONG)bytesToWrite, &ntBytesWritten);

    if (status != 0)
    {
        fprintf(stderr, "NtWriteVirtualMemory failed: 0x%lX\n", status);
        CloseHandle(hProcess);
        return FALSE;
    }

    bytesWritten = ntBytesWritten; 

    if (!VirtualProtectEx(hProcess, targetAddress, bytesToWrite, oldProtect, &oldProtect))
    {
        PrintLastError("VirtualProtectEx (restore) failed");
    }

    if (!FlushInstructionCache(hProcess, targetAddress, bytesToWrite))
    {
        PrintLastError("FlushInstructionCache failed");
    }

    msg_info_a("Successfully replaced %zu bytes at address %p (module base + 0x%lX) in process %lu.\n", bytesWritten,
               targetAddress, baseOffset, processId);

    CloseHandle(hProcess);
    return TRUE;
}
