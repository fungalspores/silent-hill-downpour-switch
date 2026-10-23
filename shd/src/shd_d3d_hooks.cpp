// Silent Hill: Downpour — hooks on the game's XDK Direct3D entry points (addresses in D3D_SHD.md).
//
// Every recompiled sub_X is a weak symbol over __imp__sub_X, so a strong definition here wraps it and the
// original always runs. The wrappers hand the draws to the native renderer (NoteDrawCall), which pairs each
// PM4 draw packet the ring thread reads with the shader objects bound at that moment.
//
// Mass Effect's equivalent is app/src/me_d3d_trace.cpp; this file keeps only what Downpour needs.
#include <cstdint>
#include <cstring>

#include <rex/logging.h>
#include <rex/ppc/context.h>
#include <rex/ppc/func.h>

#include "native/me_native_system.h"

#include <atomic>
#include <chrono>

namespace {
// Diagnostic: are these hooks reached at all, and with what? Reported every 10 s from the draws.
std::atomic<uint64_t> g_draws{0}, g_set_vs{0}, g_set_ps{0}, g_last_vs{0}, g_last_ps{0}, g_device{0};
void HookReport(const uint8_t* base) {
  static std::atomic<int64_t> next{0};
  const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::steady_clock::now().time_since_epoch()).count();
  int64_t want = next.load(std::memory_order_relaxed);
  if (now < want || !next.compare_exchange_strong(want, now + 10)) return;
  uint32_t dev = 0;
  std::memcpy(&dev, base + 0x8378F690, 4);
  dev = __builtin_bswap32(dev);
  g_device.store(dev, std::memory_order_relaxed);
  REXLOG_INFO("[shd-d3d] hooks: {} draws, SetVertexShader {} (last object {:08X}), SetPixelShader {} (last {:08X}); "
              "device pointer {:08X}",
              g_draws.load(), g_set_vs.load(), g_last_vs.load(), g_set_ps.load(), g_last_ps.load(), dev);
}
}  // namespace

namespace {
// The five draw entry points of Downpour's Direct3D. They all take the device in r3 and the primitive type in
// r4; the counts are in r5..r8 depending on the overload, which is why NoteDrawCall records several of them and
// the ring matches whichever agrees with the packet.
#define SHD_DRAWS(X) \
  X(82D1D320)        \
  X(82D1D828)        \
  X(82D1DD68)        \
  X(82D1E180)        \
  X(82D1E5F8)
}  // namespace

#define X(a)                                                                          \
  REX_EXTERN(__imp__sub_##a);                                                         \
  extern "C" REX_FUNC(sub_##a) {                                                      \
    g_draws.fetch_add(1, std::memory_order_relaxed);                                  \
    HookReport(base);                                                                 \
    me::native::NoteDrawCall(base, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32);   \
    __imp__sub_##a(ctx, base);                                                        \
  }
SHD_DRAWS(X)
#undef X

// The real shader binder of this XDK: sub_82D198C8(device r3, slot r4, object r5) writes the object into the
// device's slot table at dev+12816 + slot*4 (stwx with (r4 + 3204) * 4). The per-type setters sub_82D185F8 /
// sub_82D18560 only keep a shadow copy at dev+12416 / +12412 and the game does not call them, which is why the
// first attempt saw no objects at all. Slots: 0 = ?, 1 = pixel, 2 = vertex (checked against the type word of
// the object: 6 = vertex, 7 = pixel, see sub_82D1CA10 / sub_82D1CCF8).
REX_EXTERN(__imp__sub_82D198C8);
extern "C" REX_FUNC(sub_82D198C8) {
  const uint32_t object = ctx.r5.u32;
  if (object >= 0x40000000u && object < 0x7F000000u) {
    uint32_t type = 0;
    std::memcpy(&type, base + object, 4);
    type = __builtin_bswap32(type) & 0xFF;
    if (type == 6 || type == 7) {
      const bool vertex = type == 6;
      (vertex ? g_set_vs : g_set_ps).fetch_add(1, std::memory_order_relaxed);
      (vertex ? g_last_vs : g_last_ps).store(object, std::memory_order_relaxed);
      me::native::NoteShaderObject(base, object, vertex);
    }
  }
  __imp__sub_82D198C8(ctx, base);
}
