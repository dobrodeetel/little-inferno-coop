// Player 2 for Little Inferno.
#include <windows.h>
#include <shlobj.h>
#include <string>
#include <cmath>
#include <cstdio>
#include <share.h>
#include "pad.h"
#include "overlay.h"

extern std::wstring g_dir;
void Log(const char* fmt, ...);

static BYTE* g_exe;

// ---- settings (licoop.ini next to the exe) ----
struct Settings {
    bool enabled = true;
    std::wstring saves_dir;  // empty: the game's own %APPDATA%\Little Inferno
    float speed = 0.9f;      // screen heights per second at full stick
    float dead_zone = 0.15f;
    float cursor_size = 22;  // ring radius in pixels
    DWORD color = 0xff8c1a;  // RRGGBB
};
static Settings g_cfg;

static std::wstring IniPath() { return g_dir + L"\\licoop.ini"; }

static float IniFloat(const wchar_t* key, float def) {
    wchar_t buf[64], d[64];
    swprintf_s(d, L"%g", def);
    GetPrivateProfileStringW(L"coop", key, d, buf, 64, IniPath().c_str());
    return (float)_wtof(buf);
}

// Read at start and again every second, so the ini can be edited while playing.
static void LoadSettings() {
    std::wstring ini = IniPath();
    wchar_t buf[MAX_PATH];
    g_cfg.enabled = GetPrivateProfileIntW(L"coop", L"enabled", 1, ini.c_str()) != 0;
    GetPrivateProfileStringW(L"coop", L"saves_dir", L"", buf, MAX_PATH, ini.c_str());
    g_cfg.saves_dir = buf;
    g_cfg.speed = IniFloat(L"cursor_speed", 0.9f);
    g_cfg.dead_zone = IniFloat(L"stick_dead_zone", 0.15f);
    g_cfg.cursor_size = IniFloat(L"cursor_size", 22);
    GetPrivateProfileStringW(L"coop", L"cursor_color", L"ff8c1a", buf, MAX_PATH, ini.c_str());
    g_cfg.color = wcstoul(buf, nullptr, 16) & 0xffffff;
}

// ---- IAT hooks ----
static void** FindImport(const char* dll, const char* fn) {
    auto* nt = (IMAGE_NT_HEADERS32*)(g_exe + ((IMAGE_DOS_HEADER*)g_exe)->e_lfanew);
    auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    for (auto* d = (IMAGE_IMPORT_DESCRIPTOR*)(g_exe + dir.VirtualAddress); d->Name; d++) {
        if (_stricmp((char*)(g_exe + d->Name), dll)) continue;
        auto* name = (IMAGE_THUNK_DATA32*)(g_exe + d->OriginalFirstThunk);
        auto* addr = (IMAGE_THUNK_DATA32*)(g_exe + d->FirstThunk);
        for (; name->u1.AddressOfData; name++, addr++) {
            if (IMAGE_SNAP_BY_ORDINAL32(name->u1.Ordinal)) continue;
            auto* ibn = (IMAGE_IMPORT_BY_NAME*)(g_exe + name->u1.AddressOfData);
            if (!strcmp((char*)ibn->Name, fn)) return (void**)&addr->u1.Function;
        }
    }
    return nullptr;
}

template <class T>
static bool HookImport(const char* dll, const char* fn, T hook, T* orig) {
    void** slot = FindImport(dll, fn);
    if (!slot) { Log("import %s!%s not found", dll, fn); return false; }
    DWORD old;
    VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old);
    *orig = (T)*slot;
    *slot = (void*)hook;
    VirtualProtect(slot, sizeof(void*), old, &old);
    return true;
}

// Saves: lets a test copy keep its own profile instead of the owner's.
typedef HRESULT(WINAPI* SHGetFolderPathW_t)(HWND, int, HANDLE, DWORD, LPWSTR);
static SHGetFolderPathW_t o_SHGetFolderPathW;
static HRESULT WINAPI h_SHGetFolderPathW(HWND w, int csidl, HANDLE tok, DWORD flags, LPWSTR out) {
    if ((csidl & 0xff) == CSIDL_APPDATA && !g_cfg.saves_dir.empty()) {
        std::wstring d = g_cfg.saves_dir;
        if (d.size() < 2 || d[1] != L':') d = g_dir + L"\\" + d;
        CreateDirectoryW(d.c_str(), nullptr);
        wcsncpy_s(out, MAX_PATH, d.c_str(), _TRUNCATE);
        Log("saves redirected to %ls", d.c_str());
        return S_OK;
    }
    return o_SHGetFolderPathW(w, csidl, tok, flags, out);
}

// ---- inline hooks on game functions ----
// Addresses are Ghidra VAs (image base 0x400000); the exe is ASLR'd, so rebase them.
static BYTE* Va(DWORD va) { return g_exe + (va - 0x400000); }

// Copies `len` prologue bytes (which must not be relative jumps/calls) to a trampoline and
// writes a jmp to `hook` over them. Returns the trampoline, which runs the original function.
static void* HookCode(DWORD va, int len, const BYTE* expect, void* hook) {
    BYTE* at = Va(va);
    if (memcmp(at, expect, len)) { Log("hook %08lx: unexpected bytes, skipped", va); return nullptr; }
    BYTE* tr = (BYTE*)VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    memcpy(tr, at, len);
    tr[len] = 0xE9;
    *(DWORD*)(tr + len + 1) = (DWORD)(at + len) - (DWORD)(tr + len + 5);
    DWORD old;
    VirtualProtect(at, len, PAGE_EXECUTE_READWRITE, &old);
    at[0] = 0xE9;
    *(DWORD*)(at + 1) = (DWORD)hook - (DWORD)(at + 5);
    for (int i = 5; i < len; i++) at[i] = 0x90;
    VirtualProtect(at, len, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, len);
    return tr;
}

// Render-command executor (FUN_004a4630): `switch (*(short*)cmd)`, command 0x17 presents the
// swap chain stored at renderer+0x10 (renderer = *(void**)0x7134dc).
static void* t_RenderCmd;
static void __cdecl OnRenderCmd(short* cmd) {
    if (*cmd != 0x17) return;
    BYTE* r = *(BYTE**)Va(0x7134dc);
    if (r) OverlayBeforePresent(*(IDXGISwapChain**)(r + 0x10));
}
static __declspec(naked) void h_RenderCmd() {
    __asm {
        pushad
        push dword ptr [esp + 36]
        call OnRenderCmd
        add esp, 4
        popad
        jmp dword ptr [t_RenderCmd]
    }
}

// ---- player 2 pointer ----
struct P2 {
    float x = 0.5f, y = 0.5f;  // 0..1 across the window
    bool down = false;
    bool active = false;
    DWORD last_tick = 0;
};
static P2 g_p2;

static float DeadZone(float v, float dz) {
    float a = fabsf(v);
    if (a < dz) return 0;
    return (v < 0 ? -1 : 1) * (a - dz) / (1 - dz);
}

// Test hook: licoop_cmd.txt next to the exe holds "x y button" (x, y in 0..1). While the file
// exists it drives player 2 instead of the pad.
static bool ReadCmdFile(P2& p) {
    static DWORD last_check;
    static bool have;
    static float cx, cy;
    static int cb;
    DWORD now = GetTickCount();
    if (now - last_check > 50) {
        last_check = now;
        // A half-written file keeps the last values; only a missing file hands back to the pad.
        FILE* f = _wfsopen((g_dir + L"\\licoop_cmd.txt").c_str(), L"r", _SH_DENYNO);
        if (f) {
            float x, y;
            int b;
            if (fscanf_s(f, "%f %f %d", &x, &y, &b) == 3) { cx = x; cy = y; cb = b; have = true; }
            fclose(f);
        } else {
            have = false;
        }
    }
    if (!have) return false;
    p.x = cx; p.y = cy; p.down = cb > 0; p.active = cb >= 0;  // button -1: player 2 absent
    return true;
}

// Advances player 2 from the pad. Called once per game input tick.
static void UpdateP2() {
    DWORD now = GetTickCount();
    float dt = g_p2.last_tick ? (now - g_p2.last_tick) / 1000.0f : 0;
    if (dt > 0.1f) dt = 0.1f;
    g_p2.last_tick = now;
    static DWORD last_ini;
    if (now - last_ini > 1000) { last_ini = now; LoadSettings(); }
    if (ReadCmdFile(g_p2)) return;
    PadState s = pad::get();
    g_p2.active = s.connected;
    if (!s.connected) { g_p2.down = false; return; }
    float lx = DeadZone(s.lx, g_cfg.dead_zone), ly = DeadZone(s.ly, g_cfg.dead_zone);
    float mag = sqrtf(lx * lx + ly * ly);
    float k = g_cfg.speed * mag;  // slower near the centre for precise aiming
    g_p2.x += lx * k * dt / OverlayAspect();
    g_p2.y += ly * k * dt;
    g_p2.x = g_p2.x < 0 ? 0 : g_p2.x > 1 ? 1 : g_p2.x;
    g_p2.y = g_p2.y < 0 ? 0 : g_p2.y > 1 ? 1 : g_p2.y;
    g_p2.down = s.south || s.r2 > 0.5f;
}

// Input poll (FUN_00473e70 calls FUN_0049c830, which fills 22 float channels at [ebp-0x5c]).
// The pointer builder FUN_00461100 reads two pointers: #0 from channels 8 (active), 9 (button),
// 0xb/0xc (x/y in 0..1) and #1 from 0xe, 0xf, 0x10, 0x11, which nothing feeds on PC.
static bool InFrontEnd();
static void* t_Channels;
static void __cdecl OnChannels(float* ch) {
    UpdateP2();
    if (InFrontEnd()) { g_p2.active = false; g_p2.down = false; }
    ch[0x0e] = g_p2.active ? 1.0f : 0.0f;
    ch[0x0f] = g_p2.active && g_p2.down ? 1.0f : 0.0f;
    ch[0x10] = g_p2.x;
    ch[0x11] = g_p2.y;
}
static __declspec(naked) void h_Channels() {
    __asm {
        pushad
        lea eax, [ebp - 0x5c]
        push eax
        call OnChannels
        add esp, 4
        popad
        jmp dword ptr [t_Channels]
    }
}

// Front end (title, mode and slot select): FUN_00467330 is that scene's update; FUN_00419470 is
// the "do not play with fire" warning before it. The owner asked for player 1 only outside the
// game itself, so while either runs player 2's pointer is switched off and hidden.
static void* t_FrontEnd;
static void* t_Warning;
static volatile DWORD g_frontend_tick;
static void __cdecl OnFrontEnd() { g_frontend_tick = GetTickCount(); }
static __declspec(naked) void h_FrontEnd() {
    __asm {
        pushad
        call OnFrontEnd
        popad
        jmp dword ptr [t_FrontEnd]
    }
}
static __declspec(naked) void h_Warning() {
    __asm {
        pushad
        call OnFrontEnd
        popad
        jmp dword ptr [t_Warning]
    }
}
static bool InFrontEnd() { return GetTickCount() - g_frontend_tick < 250; }

static int OverlayFrame(OverlayPointer* out, int max) {
    P2 p = g_p2;
    if (!p.active || max < 1) return 0;
    DWORD c = g_cfg.color;
    out[0] = {p.x, p.y, g_cfg.cursor_size, (c >> 16 & 255) / 255.0f, (c >> 8 & 255) / 255.0f, (c & 255) / 255.0f, p.down};
    return 1;
}

void CoopInit(HMODULE exe) {
    g_exe = (BYTE*)exe;
    LoadSettings();
    HookImport("SHELL32.dll", "SHGetFolderPathW", h_SHGetFolderPathW, &o_SHGetFolderPathW);
    if (!g_cfg.enabled) { Log("co-op disabled in licoop.ini"); return; }
    static const BYTE kRenderCmd[] = {0x55, 0x8b, 0xec, 0x81, 0xec, 0x5c, 0x03, 0x00, 0x00};
    t_RenderCmd = HookCode(0x4a4630, sizeof(kRenderCmd), kRenderCmd, h_RenderCmd);
    static const BYTE kChannels[] = {0x8d, 0x45, 0xa4, 0x6a, 0x58};  // lea eax,[ebp-0x5c]; push 0x58
    t_Channels = HookCode(0x473ec6, sizeof(kChannels), kChannels, h_Channels);
    static const BYTE kFrontEnd[] = {0x55, 0x8b, 0xec, 0x81, 0xec, 0xa4, 0x00, 0x00, 0x00};
    t_FrontEnd = HookCode(0x467330, sizeof(kFrontEnd), kFrontEnd, h_FrontEnd);
    static const BYTE kWarning[] = {0x55, 0x8b, 0xec, 0x83, 0xec, 0x3c};
    t_Warning = HookCode(0x419470, sizeof(kWarning), kWarning, h_Warning);
    OverlaySetFrameFn(OverlayFrame);
    pad::start();
    Log("co-op ready");
}
