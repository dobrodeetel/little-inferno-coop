// Little Inferno co-op: winmm.dll proxy.
// The game imports winmm.dll statically, so this DLL is loaded before the game's code runs.
// Every winmm export jumps to the system winmm (winmm_forwards.h). On the supported build it
// adds a second, gamepad-driven pointer (see coop.cpp).
#include <windows.h>
#include <shlobj.h>
#include <cstdio>
#include <share.h>
#include <cstdarg>
#include <string>
#include "winmm_forwards.h"

std::wstring g_dir;  // game folder, with no trailing slash
static FILE* g_log;

void Log(const char* fmt, ...) {
    if (!g_log) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(g_log, "%02d:%02d:%02d.%03d ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_list a;
    va_start(a, fmt);
    vfprintf(g_log, fmt, a);
    va_end(a);
    fputc('\n', g_log);
    fflush(g_log);
}

// Supported build: Steam 221260 build 10124709, version 2.0.3.0.
static const DWORD kTimeStamp = 1670526110;
static const DWORD kSizeOfImage = 0x13D66000;

static bool IsSupportedExe(HMODULE exe) {
    auto* dos = (IMAGE_DOS_HEADER*)exe;
    auto* nt = (IMAGE_NT_HEADERS32*)((BYTE*)exe + dos->e_lfanew);
    Log("exe timestamp %lu size of image 0x%lx base %p", nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage, exe);
    return nt->FileHeader.TimeDateStamp == kTimeStamp && nt->OptionalHeader.SizeOfImage == kSizeOfImage;
}

void CoopInit(HMODULE exe);  // coop.cpp

// Logs crashes with the module and offset, so a crash report says where it happened.
static LONG WINAPI CrashLogger(EXCEPTION_POINTERS* e) {
    DWORD code = e->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_ILLEGAL_INSTRUCTION && code != EXCEPTION_STACK_OVERFLOW &&
        code != EXCEPTION_INT_DIVIDE_BY_ZERO && code != 0xC0000409)
        return EXCEPTION_CONTINUE_SEARCH;
    void* at = e->ExceptionRecord->ExceptionAddress;
    HMODULE m = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)at, &m);
    wchar_t name[MAX_PATH] = L"?";
    if (m) GetModuleFileNameW(m, name, MAX_PATH);
    auto* c = e->ContextRecord;
    Log("EXCEPTION %08lx at %p (%ls+0x%lx) addr %p eax %08lx ecx %08lx edx %08lx esi %08lx edi %08lx esp %08lx", code, at,
        wcsrchr(name, L'\\') ? wcsrchr(name, L'\\') + 1 : name, (DWORD)((BYTE*)at - (BYTE*)m),
        e->ExceptionRecord->NumberParameters > 1 ? (void*)e->ExceptionRecord->ExceptionInformation[1] : nullptr, c->Eax, c->Ecx,
        c->Edx, c->Esi, c->Edi, c->Esp);
    DWORD* sp = (DWORD*)c->Esp;
    for (int i = 0; i < 24; i++) {
        HMODULE sm = nullptr;
        if (!IsBadReadPtr(sp + i, 4) &&
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)sp[i], &sm) &&
            sm) {
            GetModuleFileNameW(sm, name, MAX_PATH);
            Log("  stack[%d] %08lx %ls+0x%lx", i, sp[i], wcsrchr(name, L'\\') + 1, sp[i] - (DWORD)sm);
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, void*) {
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(self);
    wchar_t sys[MAX_PATH];
    GetSystemDirectoryW(sys, MAX_PATH);  // SysWOW64 for a 32-bit process
    HMODULE real = LoadLibraryW((std::wstring(sys) + L"\\winmm.dll").c_str());
    if (!real) return FALSE;
    winmm_resolve(real);

    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    g_dir = path;
    g_dir = g_dir.substr(0, g_dir.find_last_of(L"\\/"));
    g_log = _wfsopen((g_dir + L"\\licoop.log").c_str(), L"w", _SH_DENYNO);
    Log("Little Inferno co-op loaded");
    AddVectoredExceptionHandler(1, CrashLogger);
    HMODULE exe = GetModuleHandleW(nullptr);
    if (!IsSupportedExe(exe)) {
        Log("unsupported game build, co-op disabled");
        return TRUE;
    }
    CoopInit(exe);
    return TRUE;
}
