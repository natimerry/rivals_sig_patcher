#include <msg.h>
#include <windows.h>
#include <patcher.h>
#include <tlhelp32.h>
#include <psapi.h> // For GetModuleInformation
#include <stdio.h>
#include <game.h>
void PrintLastError(const char* msg)
{
    DWORD err = GetLastError();
    char* errMsg = NULL;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                   err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&errMsg, 0, NULL);
    printf("%s (Error %lu): %s", msg, err, errMsg);
    LocalFree(errMsg);
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
