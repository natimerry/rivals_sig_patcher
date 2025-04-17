#ifndef GAME_H
#define GAME_H

#include <windows.h>

DWORD get_game_handle();

HMODULE GetImageBaseNtQuery(HANDLE hProcess);

HMODULE GetModuleBaseAddress(DWORD processId);

#endif