#pragma once
#include <windows.h>

const int kMaxOverlayPointers = 4;

struct OverlayPointer {
    float x, y;         // 0..1 across the back buffer
    float radius_px;
    float r, g, b;
    bool pressed;
};

// Called once per frame from Present; fills up to max pointers and returns the count.
typedef int (*OverlayFrameFn)(OverlayPointer* out, int max);
void OverlaySetFrameFn(OverlayFrameFn fn);

// Width / height of the last presented frame (16:9 until the first frame).
float OverlayAspect();

struct IDXGISwapChain;
void OverlayBeforePresent(IDXGISwapChain* sc);
