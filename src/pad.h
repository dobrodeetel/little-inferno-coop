// Gamepad reader for player 2: DualSense / DualShock 4 over raw HID, anything else via XInput.
// The game has no gamepad code, so the mod reads the pad itself on a background thread.
#pragma once
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <atomic>
#include <cstdint>
#include <cstring>

struct PadState {
    bool connected = false;
    float lx = 0, ly = 0, rx = 0, ry = 0;  // -1..1, y down positive
    float l2 = 0, r2 = 0;                  // 0..1
    bool south = false, east = false, west = false, north = false;  // Cross, Circle, Square, Triangle
    bool l1 = false, r1 = false, l3 = false, r3 = false;
    bool start = false, back = false;
    bool up = false, down = false, left = false, right = false;
};

void Log(const char* fmt, ...);

namespace pad {

enum Kind { NONE, DUALSENSE, DS4 };

static SRWLOCK g_lock = SRWLOCK_INIT;
static PadState g_state;
static std::atomic<bool> g_run{false};
static HANDLE g_thread = nullptr;

static float axis(uint8_t v) { return v < 128 ? (v - 128) / 128.0f : (v - 128) / 127.0f; }

static void dpad(PadState& s, int hat) {
    s.up = hat == 7 || hat == 0 || hat == 1;
    s.right = hat >= 1 && hat <= 3;
    s.down = hat >= 3 && hat <= 5;
    s.left = hat >= 5 && hat <= 7;
}

// USB input report 0x01 (and the Bluetooth 0x31 report, shifted by one byte).
static bool parse(Kind kind, const uint8_t* r, DWORD n, PadState& s) {
    if (n < 11) return false;
    const uint8_t* p;
    if (r[0] == 0x01 && n >= 33) p = r + 1;
    else if (r[0] == 0x31 && n >= 34) p = r + 2;
    else if (r[0] == 0x11 && n >= 34) p = r + 3;  // DS4 Bluetooth full report
    else return false;
    s.connected = true;
    s.lx = axis(p[0]); s.ly = axis(p[1]); s.rx = axis(p[2]); s.ry = axis(p[3]);
    uint8_t b0, b1;
    if (kind == DUALSENSE) {
        s.l2 = p[4] / 255.0f; s.r2 = p[5] / 255.0f;
        b0 = p[7]; b1 = p[8];
    } else {
        b0 = p[4]; b1 = p[5];
        s.l2 = p[7] / 255.0f; s.r2 = p[8] / 255.0f;
    }
    dpad(s, b0 & 0x0f);
    s.west = b0 & 0x10; s.south = b0 & 0x20; s.east = b0 & 0x40; s.north = b0 & 0x80;
    s.l1 = b1 & 0x01; s.r1 = b1 & 0x02; s.back = b1 & 0x10; s.start = b1 & 0x20;
    s.l3 = b1 & 0x40; s.r3 = b1 & 0x80;
    return true;
}

static HANDLE open_hid(Kind& kind) {
    GUID guid;
    HidD_GetHidGuid(&guid);
    HDEVINFO set = SetupDiGetClassDevsW(&guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return INVALID_HANDLE_VALUE;
    HANDLE found = INVALID_HANDLE_VALUE;
    SP_DEVICE_INTERFACE_DATA ifd{sizeof(ifd)};
    for (DWORD i = 0; found == INVALID_HANDLE_VALUE && SetupDiEnumDeviceInterfaces(set, nullptr, &guid, i, &ifd); i++) {
        DWORD need = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &ifd, nullptr, 0, &need, nullptr);
        auto* det = (SP_DEVICE_INTERFACE_DETAIL_DATA_W*)malloc(need);
        det->cbSize = sizeof(*det);
        if (SetupDiGetDeviceInterfaceDetailW(set, &ifd, det, need, nullptr, nullptr)) {
            HANDLE h = CreateFileW(det->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
            if (h != INVALID_HANDLE_VALUE) {
                HIDD_ATTRIBUTES a{sizeof(a)};
                HidD_GetAttributes(h, &a);
                Kind k = NONE;
                if (a.VendorID == 0x054C && (a.ProductID == 0x0CE6 || a.ProductID == 0x0DF2)) k = DUALSENSE;
                if (a.VendorID == 0x054C && (a.ProductID == 0x05C4 || a.ProductID == 0x09CC || a.ProductID == 0x0BA0)) k = DS4;
                PHIDP_PREPARSED_DATA pp;
                bool gamepad = false;
                if (k != NONE && HidD_GetPreparsedData(h, &pp)) {
                    HIDP_CAPS caps;
                    if (HidP_GetCaps(pp, &caps) == HIDP_STATUS_SUCCESS) gamepad = caps.UsagePage == 1 && caps.Usage == 5;
                    HidD_FreePreparsedData(pp);
                }
                if (gamepad) { found = h; kind = k; }
                else CloseHandle(h);
            }
        }
        free(det);
    }
    SetupDiDestroyDeviceInfoList(set);
    return found;
}

typedef struct { WORD wButtons; BYTE lt, rt; SHORT lx, ly, rx, ry; } XGamepad;
typedef struct { DWORD packet; XGamepad g; } XState;
typedef DWORD(WINAPI* XInputGetState_t)(DWORD, XState*);

static bool read_xinput(XInputGetState_t get, PadState& s) {
    if (!get) return false;
    for (DWORD i = 0; i < 4; i++) {
        XState x{};
        if (get(i, &x) != 0) continue;
        auto ax = [](SHORT v) { return v < 0 ? v / 32768.0f : v / 32767.0f; };
        s = PadState{};
        s.connected = true;
        s.lx = ax(x.g.lx); s.ly = -ax(x.g.ly); s.rx = ax(x.g.rx); s.ry = -ax(x.g.ry);
        s.l2 = x.g.lt / 255.0f; s.r2 = x.g.rt / 255.0f;
        WORD b = x.g.wButtons;
        s.up = b & 1; s.down = b & 2; s.left = b & 4; s.right = b & 8;
        s.start = b & 0x10; s.back = b & 0x20; s.l3 = b & 0x40; s.r3 = b & 0x80;
        s.l1 = b & 0x100; s.r1 = b & 0x200;
        s.south = b & 0x1000; s.east = b & 0x2000; s.west = b & 0x4000; s.north = b & 0x8000;
        return true;
    }
    return false;
}

static DWORD WINAPI thread_main(void*) {
    HMODULE xi = LoadLibraryW(L"xinput1_4.dll");
    if (!xi) xi = LoadLibraryW(L"xinput9_1_0.dll");
    auto xget = xi ? (XInputGetState_t)GetProcAddress(xi, "XInputGetState") : nullptr;
    HANDLE hid = INVALID_HANDLE_VALUE;
    Kind kind = NONE;
    DWORD last_scan = 0;
    uint8_t buf[128];
    OVERLAPPED ov{};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    while (g_run) {
        if (hid == INVALID_HANDLE_VALUE && GetTickCount() - last_scan > 2000) {
            last_scan = GetTickCount();
            hid = open_hid(kind);
            if (hid != INVALID_HANDLE_VALUE) Log("pad: %s opened over HID", kind == DUALSENSE ? "DualSense" : "DualShock 4");
        }
        if (hid != INVALID_HANDLE_VALUE) {
            ResetEvent(ov.hEvent);
            DWORD n = 0;
            if (!ReadFile(hid, buf, sizeof(buf), nullptr, &ov) && GetLastError() != ERROR_IO_PENDING) {
                Log("pad: HID read failed (%lu), closing", GetLastError());
                CloseHandle(hid); hid = INVALID_HANDLE_VALUE;
                continue;
            }
            if (WaitForSingleObject(ov.hEvent, 200) != WAIT_OBJECT_0) { CancelIo(hid); continue; }
            if (!GetOverlappedResult(hid, &ov, &n, FALSE)) { CloseHandle(hid); hid = INVALID_HANDLE_VALUE; continue; }
            PadState s;
            static bool logged_report;
            if (!logged_report) { logged_report = true; Log("pad: first report id 0x%02x, %lu bytes", buf[0], n); }
            if (parse(kind, buf, n, s)) {
                AcquireSRWLockExclusive(&g_lock); g_state = s; ReleaseSRWLockExclusive(&g_lock);
            }
            continue;
        }
        PadState s;
        read_xinput(xget, s);
        AcquireSRWLockExclusive(&g_lock); g_state = s; ReleaseSRWLockExclusive(&g_lock);
        Sleep(8);
    }
    if (hid != INVALID_HANDLE_VALUE) CloseHandle(hid);
    return 0;
}

static void start() {
    if (g_run.exchange(true)) return;
    g_thread = CreateThread(nullptr, 0, thread_main, nullptr, 0, nullptr);
}

static PadState get() {
    AcquireSRWLockShared(&g_lock);
    PadState s = g_state;
    ReleaseSRWLockShared(&g_lock);
    return s;
}

}  // namespace pad
