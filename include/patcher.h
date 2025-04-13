#ifndef PATCHER_H
#define PATCHER_H
#include <stddef.h>
#include <stdint.h>
#include <windows.h>

typedef struct
{
    BYTE* data;
    SIZE_T size;
} Pattern;

LPVOID find_pattern(HANDLE hProcess, LPCVOID baseAddress, SIZE_T regionSize, const Pattern* pattern);

typedef struct
{
    BYTE* data;
    SIZE_T size;
} Instruction;

BOOL ReplaceInstructionInProcess(DWORD processId, DWORD baseOffset, const Instruction* newInstruction);

#endif
