// Draws player 2's pointer on top of each frame.
// coop.cpp hooks the game's render-command executor and calls OverlayBeforePresent with the
// game's swap chain when the Present command comes up. (Patching the IDXGISwapChain vtable
// crashed inside the Steam overlay, which hooks Present too.)
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <cmath>
#include "overlay.h"

void Log(const char* fmt, ...);

typedef HRESULT(WINAPI* D3DCompile_t)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*, ID3DInclude*, LPCSTR, LPCSTR, UINT, UINT,
                                      ID3DBlob**, ID3DBlob**);

static OverlayFrameFn g_frame_fn;
static float g_aspect = 16.0f / 9.0f;

float OverlayAspect() { return g_aspect; }

struct Gpu {
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    ID3D11VertexShader* vs = nullptr;
    ID3D11PixelShader* ps = nullptr;
    ID3D11InputLayout* il = nullptr;
    ID3D11Buffer* vb = nullptr;
    ID3D11BlendState* blend = nullptr;
    ID3D11RasterizerState* rs = nullptr;
    ID3D11DepthStencilState* ds = nullptr;
    bool ready = false, failed = false;
};
static Gpu g;

// One quad per pointer. The pixel shader draws a ring with a dot, coloured per vertex.
static const char* kShader = R"(
struct V { float2 pos : POSITION; float2 uv : TEXCOORD; float4 col : COLOR; };
struct P { float4 pos : SV_Position; float2 uv : TEXCOORD; float4 col : COLOR; };
P vs(V v) { P p; p.pos = float4(v.pos, 0, 1); p.uv = v.uv; p.col = v.col; return p; }
float disc(float r, float rad, float aa) { return 1 - smoothstep(rad - aa, rad, r); }
// Premultiplied output: coloured ring and dot, thin dark outlines, a soft fill while pressed.
float4 ps(P p) : SV_Target {
    float r = length(p.uv);
    float aa = max(fwidth(r), 0.001);
    float ring = disc(r, 0.80, aa) - disc(r, 0.62, aa);
    float outline = disc(r, 0.94, aa) - disc(r, 0.80, aa) + disc(r, 0.62, aa) - disc(r, 0.52, aa);
    float dot_ = disc(r, 0.18, aa);
    float dot_outline = disc(r, 0.28, aa) - dot_;
    float fill = (disc(r, 0.52, aa) - disc(r, 0.28, aa)) * p.col.a * 0.45;
    float lit = ring + dot_ + fill;
    float a = saturate(lit + 0.8 * (outline + dot_outline));
    return float4(p.col.rgb * lit, a);
}
)";

struct Vtx { float x, y, u, v, r, gc, b, a; };

static bool InitGpu(IDXGISwapChain* sc) {
    if (g.ready || g.failed) return g.ready;
    g.failed = true;
    if (FAILED(sc->GetDevice(__uuidof(ID3D11Device), (void**)&g.dev))) return false;
    g.dev->GetImmediateContext(&g.ctx);
    HMODULE dc = LoadLibraryW(L"d3dcompiler_47.dll");
    auto compile = dc ? (D3DCompile_t)GetProcAddress(dc, "D3DCompile") : nullptr;
    if (!compile) { Log("overlay: d3dcompiler_47 missing"); return false; }
    ID3DBlob *vsb = nullptr, *psb = nullptr, *err = nullptr;
    if (FAILED(compile(kShader, strlen(kShader), "p2", nullptr, nullptr, "vs", "vs_4_0", 0, 0, &vsb, &err)) ||
        FAILED(compile(kShader, strlen(kShader), "p2", nullptr, nullptr, "ps", "ps_4_0", 0, 0, &psb, &err))) {
        Log("overlay: shader error %s", err ? (char*)err->GetBufferPointer() : "?");
        return false;
    }
    g.dev->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &g.vs);
    g.dev->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &g.ps);
    D3D11_INPUT_ELEMENT_DESC el[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    g.dev->CreateInputLayout(el, 3, vsb->GetBufferPointer(), vsb->GetBufferSize(), &g.il);
    D3D11_BUFFER_DESC bd{sizeof(Vtx) * 6 * kMaxOverlayPointers, D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE};
    g.dev->CreateBuffer(&bd, nullptr, &g.vb);
    D3D11_BLEND_DESC bl{};
    auto& rt = bl.RenderTarget[0];
    rt.BlendEnable = TRUE;
    rt.SrcBlend = D3D11_BLEND_ONE; rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA; rt.BlendOp = D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D11_BLEND_ONE; rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA; rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    g.dev->CreateBlendState(&bl, &g.blend);
    D3D11_RASTERIZER_DESC rd{D3D11_FILL_SOLID, D3D11_CULL_NONE};
    rd.DepthClipEnable = TRUE;
    g.dev->CreateRasterizerState(&rd, &g.rs);
    D3D11_DEPTH_STENCIL_DESC dd{};
    g.dev->CreateDepthStencilState(&dd, &g.ds);
    if (!g.vs || !g.ps || !g.il || !g.vb || !g.blend || !g.rs || !g.ds) { Log("overlay: resource creation failed"); return false; }
    g.failed = false;
    g.ready = true;
    Log("overlay ready");
    return true;
}

static void Draw(IDXGISwapChain* sc) {
    DXGI_SWAP_CHAIN_DESC scd;
    if (SUCCEEDED(sc->GetDesc(&scd)) && scd.BufferDesc.Height) g_aspect = (float)scd.BufferDesc.Width / scd.BufferDesc.Height;
    if (!g_frame_fn) return;
    OverlayPointer ptrs[kMaxOverlayPointers];
    int n = g_frame_fn(ptrs, kMaxOverlayPointers);
    if (n <= 0 || !InitGpu(sc)) return;

    ID3D11Texture2D* bb = nullptr;
    if (FAILED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb))) return;
    D3D11_TEXTURE2D_DESC td;
    bb->GetDesc(&td);
    ID3D11RenderTargetView* rtv = nullptr;
    g.dev->CreateRenderTargetView(bb, nullptr, &rtv);
    bb->Release();
    if (!rtv) return;

    D3D11_MAPPED_SUBRESOURCE m;
    if (FAILED(g.ctx->Map(g.vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) { rtv->Release(); return; }
    auto* v = (Vtx*)m.pData;
    for (int i = 0; i < n; i++) {
        const auto& p = ptrs[i];
        float rx = p.radius_px / td.Width * 2, ry = p.radius_px / td.Height * 2;
        float cx = p.x * 2 - 1, cy = 1 - p.y * 2;
        Vtx q[4] = {{cx - rx, cy + ry, -1, -1}, {cx + rx, cy + ry, 1, -1}, {cx - rx, cy - ry, -1, 1}, {cx + rx, cy - ry, 1, 1}};
        for (auto& c : q) { c.r = p.r; c.gc = p.g; c.b = p.b; c.a = p.pressed ? 1.0f : 0.0f; }
        Vtx tri[6] = {q[0], q[1], q[2], q[2], q[1], q[3]};
        memcpy(v + i * 6, tri, sizeof(tri));
    }
    g.ctx->Unmap(g.vb, 0);

    // Save what we touch.
    ID3D11RenderTargetView* old_rtv = nullptr; ID3D11DepthStencilView* old_dsv = nullptr;
    g.ctx->OMGetRenderTargets(1, &old_rtv, &old_dsv);
    ID3D11BlendState* old_blend = nullptr; float old_bf[4]; UINT old_mask;
    g.ctx->OMGetBlendState(&old_blend, old_bf, &old_mask);
    ID3D11DepthStencilState* old_ds = nullptr; UINT old_ref;
    g.ctx->OMGetDepthStencilState(&old_ds, &old_ref);
    ID3D11RasterizerState* old_rs = nullptr; g.ctx->RSGetState(&old_rs);
    UINT nvp = 1; D3D11_VIEWPORT old_vp{}; g.ctx->RSGetViewports(&nvp, &old_vp);
    ID3D11VertexShader* old_vs = nullptr; g.ctx->VSGetShader(&old_vs, nullptr, nullptr);
    ID3D11PixelShader* old_ps = nullptr; g.ctx->PSGetShader(&old_ps, nullptr, nullptr);
    ID3D11InputLayout* old_il = nullptr; g.ctx->IAGetInputLayout(&old_il);
    D3D11_PRIMITIVE_TOPOLOGY old_topo; g.ctx->IAGetPrimitiveTopology(&old_topo);
    ID3D11Buffer* old_vb = nullptr; UINT old_stride, old_off; g.ctx->IAGetVertexBuffers(0, 1, &old_vb, &old_stride, &old_off);

    D3D11_VIEWPORT vp{0, 0, (float)td.Width, (float)td.Height, 0, 1};
    g.ctx->OMSetRenderTargets(1, &rtv, nullptr);
    float bf[4] = {};
    g.ctx->OMSetBlendState(g.blend, bf, 0xffffffff);
    g.ctx->OMSetDepthStencilState(g.ds, 0);
    g.ctx->RSSetState(g.rs);
    g.ctx->RSSetViewports(1, &vp);
    g.ctx->VSSetShader(g.vs, nullptr, 0);
    g.ctx->PSSetShader(g.ps, nullptr, 0);
    g.ctx->IASetInputLayout(g.il);
    g.ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    UINT stride = sizeof(Vtx), off = 0;
    g.ctx->IASetVertexBuffers(0, 1, &g.vb, &stride, &off);
    g.ctx->Draw(6 * n, 0);

    g.ctx->OMSetRenderTargets(1, &old_rtv, old_dsv);
    g.ctx->OMSetBlendState(old_blend, old_bf, old_mask);
    g.ctx->OMSetDepthStencilState(old_ds, old_ref);
    g.ctx->RSSetState(old_rs);
    if (nvp) g.ctx->RSSetViewports(1, &old_vp);
    g.ctx->VSSetShader(old_vs, nullptr, 0);
    g.ctx->PSSetShader(old_ps, nullptr, 0);
    g.ctx->IASetInputLayout(old_il);
    g.ctx->IASetPrimitiveTopology(old_topo);
    g.ctx->IASetVertexBuffers(0, 1, &old_vb, &old_stride, &old_off);
    IUnknown* held[] = {old_rtv, old_dsv, old_blend, old_ds, old_rs, old_vs, old_ps, old_il, old_vb};
    for (IUnknown* u : held)
        if (u) u->Release();
    rtv->Release();
}

void OverlaySetFrameFn(OverlayFrameFn fn) { g_frame_fn = fn; }

// Called by the render-command hook right before the game presents.
void OverlayBeforePresent(IDXGISwapChain* sc) {
    if (sc) Draw(sc);
}
