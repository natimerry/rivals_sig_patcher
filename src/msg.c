#include <windows.h>
#include <stdio.h>
#include <config.h>

#include <msg.h>

#define DEF_MSG_FN(name, type, printfn, mbfn, projname, flags, suffix) \
    void name(const type* format, ...)                                 \
    {                                                                  \
        va_list args;                                                  \
        va_start(args, format);                                        \
                                                                       \
        int count = printfn(NULL, 0, format, args) + 1;                \
                                                                       \
        type* buf = malloc(count * sizeof(type));                      \
        printfn(buf, count, format, args);                             \
                                                                       \
        mbfn(NULL, buf, projname, flags);                              \
                                                                       \
        va_end(args);                                                  \
                                                                       \
        free(buf);                                                     \
        suffix;                                                        \
    }

const char* TITLE_A = "v" PATCHER_VERSION " Signature Patcher";
const wchar_t* TITLE_W = L"v" PATCHER_VERSION " Signature Patcher";

// Error
DEF_MSG_FN(msg_err_a, char, _vsnprintf, MessageBoxA, TITLE_A, MB_OK | MB_ICONERROR, exit(1))
DEF_MSG_FN(msg_err_w, wchar_t, _vsnwprintf, MessageBoxW, TITLE_W, MB_OK | MB_ICONERROR, exit(1))

// Warn
DEF_MSG_FN(msg_warn_a, char, _vsnprintf, MessageBoxA, TITLE_A, MB_OK | MB_ICONEXCLAMATION, return)
DEF_MSG_FN(msg_warn_w, wchar_t, _vsnwprintf, MessageBoxW, TITLE_W, MB_OK | MB_ICONEXCLAMATION, return)

// Info
DEF_MSG_FN(msg_info_a, char, _vsnprintf, MessageBoxA, TITLE_A, MB_OK | MB_ICONINFORMATION, return)
DEF_MSG_FN(msg_info_w, wchar_t, _vsnwprintf, MessageBoxW, TITLE_W, MB_OK | MB_ICONINFORMATION, return)
