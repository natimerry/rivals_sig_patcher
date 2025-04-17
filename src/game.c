#include <stdbool.h>
#include <game.h>
#include <stdio.h>
#include <stdbool.h>
#include <windows.h>
#include <tlhelp32.h>
#include <msg.h>

#ifndef TEST_BUILD
const wchar_t* proc_name = L"Marvel-Win64-Shipping.exe";
#else
const wchar_t* proc_name = L"test2.exe";
#endif

static bool is_game_exe(const wchar_t* exe)
{
    if (wcsicmp(exe, proc_name) == 0)
    {
        return true;
    }
    return false;
}

DWORD get_game_handle()
{
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snapshot == INVALID_HANDLE_VALUE)
    {
        fprintf(stderr, "Failed to create process snapshot. Error Code: %lu\n", GetLastError());
        return 0;
    }

    if (!Process32FirstW(snapshot, &entry))
    {
        fprintf(stderr, "Could not iterate processes. Error Code: %lu\n", GetLastError());
        CloseHandle(snapshot);
        return 0;
    }

    DWORD processId = 0;
    do
    {
        if (is_game_exe(entry.szExeFile))
        {
            processId = entry.th32ProcessID;
            break;
        }
    } while (Process32NextW(snapshot, &entry));

    CloseHandle(snapshot);

    return processId;
}

#include <winternl.h>

typedef NTSTATUS(WINAPI* pNtQueryInformationProcess)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);

HMODULE GetImageBaseNtQuery(HANDLE hProcess)
{
    HMODULE hMod = NULL;

    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll)
        return NULL;

    pNtQueryInformationProcess NtQueryInformationProcess =
        (pNtQueryInformationProcess)GetProcAddress(ntdll, "NtQueryInformationProcess");

    if (!NtQueryInformationProcess)
        return NULL;

    PROCESS_BASIC_INFORMATION pbi = {0};
    if (NtQueryInformationProcess(hProcess, ProcessBasicInformation, &pbi, sizeof(pbi), NULL) != 0)
        return NULL;

#ifdef _WIN64
    PVOID pebBase = pbi.PebBaseAddress;
    PVOID imageBase = 0;
    SIZE_T read = 0;
    if (ReadProcessMemory(hProcess, (BYTE*)pebBase + 0x10, &imageBase, sizeof(imageBase), &read) &&
        read == sizeof(imageBase))
        hMod = (HMODULE)imageBase;
#else
    // For 32-bit, offset is different
    PVOID pebBase = pbi.PebBaseAddress;
    PVOID imageBase = 0;
    SIZE_T read = 0;
    if (ReadProcessMemory(hProcess, (BYTE*)pebBase + 0x08, &imageBase, sizeof(imageBase), &read) &&
        read == sizeof(imageBase))
        hMod = (HMODULE)imageBase;
#endif

    return hMod;
}