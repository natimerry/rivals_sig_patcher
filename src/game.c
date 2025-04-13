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

// Get base address of a module in a target process
HMODULE GetModuleBaseAddress(DWORD processId)
{
    HMODULE hMod = NULL;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
    if (hSnapshot != INVALID_HANDLE_VALUE)
    {
        MODULEENTRY32W modEntry;
        modEntry.dwSize = sizeof(modEntry);

        if (Module32FirstW(hSnapshot, &modEntry))
        {
            do
            {
                wprintf(L"Checking module: %s\n", modEntry.szModule);
                if (_wcsicmp(modEntry.szModule, proc_name) == 0)
                {
                    hMod = modEntry.hModule;
                    break;
                }
            } while (Module32NextW(hSnapshot, &modEntry));
        }

        CloseHandle(hSnapshot);
    }

    // If failed to get module base via snapshot (likely due to suspended process), fall back
    if (!hMod)
    {
        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
        HANDLE hThread = NULL;

        // Grab main thread
        THREADENTRY32 te32 = {.dwSize = sizeof(THREADENTRY32)};
        HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

        if (hThreadSnap != INVALID_HANDLE_VALUE)
        {
            if (Thread32First(hThreadSnap, &te32))
            {
                do
                {
                    if (te32.th32OwnerProcessID == processId)
                    {
                        hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, te32.th32ThreadID);
                        break;
                    }
                } while (Thread32Next(hThreadSnap, &te32));
            }
            CloseHandle(hThreadSnap);
        }

        if (hProcess && hThread)
        {
            CONTEXT ctx = {0};
            ctx.ContextFlags = CONTEXT_FULL;

            if (GetThreadContext(hThread, &ctx))
            {
#ifdef _WIN64
                DWORD64 pebAddress = ctx.Rdx;
                DWORD64 imageBase = 0;
                ReadProcessMemory(hProcess, (LPCVOID)(pebAddress + 0x10), &imageBase, sizeof(imageBase), NULL);
                hMod = (HMODULE)imageBase;
#else
                DWORD pebAddress = ctx.Ebx;
                DWORD imageBase = 0;
                ReadProcessMemory(hProcess, (LPCVOID)(pebAddress + 0x08), &imageBase, sizeof(imageBase), NULL);
                hMod = (HMODULE)imageBase;
#endif
            }

            CloseHandle(hThread);
        }

        if (hProcess)
        {
            CloseHandle(hProcess);
        }
    }

    return hMod;
}
