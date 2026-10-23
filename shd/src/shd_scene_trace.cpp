// Silent Hill: Downpour — what the engine's frame is made of (diagnostic, shd_diag_scene_trace).
//
// UE3 (SceneRenderTargets, sub_8284E198) creates every buffer of the frame by name through
// RHICreateTargetableSurface = sub_829CC6D8(sret r3, width r4, height r5, pixel format r6, texture r7, name r8,
// ...). That calls Direct3D's CreateRenderTarget sub_82D1BFB0, which stores the surface's EDRAM base (in tiles)
// in bits 0-11 of the surface object at +28 (rlwimi r10,r30,0,20,31; stw r10,28(r31)).
//
// So a name ("ShadowDepthRT", "DefaultColorFixedPoint", "LightAttenuation"...) can be tied to the EDRAM base the
// renderer's log talks about (2D0, 5A0, 658...), and the transfers between "formats" become transfers between
// named engine buffers.
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>
#include <rex/ppc/func.h>

REXCVAR_DEFINE_BOOL(shd_diag_scene_trace, true, "Silent Hill",
                    "Diagnostic, no image change: log every frame buffer the engine creates (name, size, UE3 pixel "
                    "format, EDRAM base, D3D surface format) once")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace {
uint32_t Load32(const uint8_t* base, uint32_t a) {
  uint32_t v;
  std::memcpy(&v, base + a, 4);
  return __builtin_bswap32(v);
}
std::string Utf16Be(const uint8_t* base, uint32_t a) {
  std::string out;
  if (a < 0x40000000u || a >= 0x9FFFFFFFu) return "?";
  for (uint32_t i = 0; i < 64; ++i) {
    const uint32_t c = (uint32_t(base[a + i * 2]) << 8) | base[a + i * 2 + 1];
    if (!c) break;
    out += c < 0x80 ? char(c) : '?';
  }
  return out;
}
std::mutex g_mutex;
std::string g_name;  // name of the RHICreateTargetableSurface in progress
uint32_t g_w = 0, g_h = 0, g_pf = 0;
}  // namespace

// RHICreateTargetableSurface(sret, w, h, pixel format, texture, name, ...)
REX_EXTERN(__imp__sub_829CC6D8);
bool ShdScaleSurfaceSize(uint32_t& w, uint32_t& h);
void ShdLogRenderScaleOnce();

extern "C" REX_FUNC(sub_829CC6D8) {
  // shd_render_width / shd_render_height: the engine is asked for smaller scene buffers, and the
  // presenter scales the frame to the screen. Done before the call so everything the engine
  // derives from this surface (half- and quarter-size effect buffers) follows.
  {
    uint32_t w = ctx.r4.u32, h = ctx.r5.u32;
    if (ShdScaleSurfaceSize(w, h)) {
      ctx.r4.u32 = w;
      ctx.r5.u32 = h;
      ShdLogRenderScaleOnce();
    }
  }
  const bool on = REXCVAR_GET(shd_diag_scene_trace);
  if (on) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_name = Utf16Be(base, ctx.r8.u32);
    g_w = ctx.r4.u32;
    g_h = ctx.r5.u32;
    g_pf = ctx.r6.u32;
  }
  __imp__sub_829CC6D8(ctx, base);
  if (on) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_name.clear();
  }
}

// D3D CreateRenderTarget(w r3, h r4, format r5, ...) -> surface object r3 (EDRAM base at +28 bits 0-11)
REX_EXTERN(__imp__sub_82D1BFB0);
extern "C" REX_FUNC(sub_82D1BFB0) {
  const uint32_t w = ctx.r3.u32, h = ctx.r4.u32, d3d_format = ctx.r5.u32;
  __imp__sub_82D1BFB0(ctx, base);
  if (!REXCVAR_GET(shd_diag_scene_trace)) return;
  const uint32_t surface = ctx.r3.u32;
  std::string name;
  uint32_t pf = 0;
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    name = g_name;
    pf = g_pf;
  }
  if (surface < 0x40000000u || surface >= 0x9FFFFFFFu) return;
  const uint32_t word28 = Load32(base, surface + 28);
  REXLOG_INFO("[shd-scene] surface '{}' {}x{} UE3 PF {} D3D format {:08X} -> EDRAM base {:03X} (+28 {:08X}, +32 {:08X}) "
              "object {:08X}",
              name.empty() ? "(not from SceneRenderTargets)" : name, w, h, pf, d3d_format, word28 & 0xFFF, word28,
              Load32(base, surface + 32), surface);
}
