#include <stdbool.h>
#include <game.h>
#include <stdio.h>
#include <stdbool.h>
#include <windows.h>
#include <tlhelp32.h>
#include <msg.h>

const wchar_t* proc_name = L"Marvel-Win64-Shipping.exe";

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

// Get base address of a module in a target process
HMODULE GetModuleBaseAddress(DWORD processId)
{
    HMODULE hMod = NULL;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
    if (hSnapshot == INVALID_HANDLE_VALUE)
    {
        return NULL;
    }

    MODULEENTRY32W modEntry;
    modEntry.dwSize = sizeof(modEntry);

    if (Module32FirstW(hSnapshot, &modEntry))
    {
        do
        {
            if (_wcsicmp(modEntry.szModule, proc_name) == 0)
            {
                hMod = modEntry.hModule;
                break;
            }
        } while (Module32NextW(hSnapshot, &modEntry));
    }

    CloseHandle(hSnapshot);
    return hMod;
}