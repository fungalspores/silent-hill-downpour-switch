// Internal render resolution for the native renderer (shd_render_width / shd_render_height).
//
// The contract, as sa2-nx states it: a RenderTarget's width and height stay the GUEST size, so
// every piece of guest logic (EDRAM tiles and their ownership, resolve rectangles, texture
// identity by address and size, 1/size in the texture constants) is unchanged. Only the Vulkan
// commands use host coordinates, converted here.
//
// The scale is kept as quarters q, where q == 4 is the game's own resolution and the whole path
// is identity: not one instruction differs from the renderer without this feature. That is what
// makes it safe to leave in all the time.
//
// A guest pixel EDGE e maps to the host edge E(e) = floor((2*e*q + 3) / 8). Edges, not sizes:
// adjacent guest rectangles then cover disjoint host pixels with no gap and no overlap, which
// scaling widths independently cannot promise.
#ifndef SHD_NATIVE_RENDER_SCALE_H_
#define SHD_NATIVE_RENDER_SCALE_H_

#include <cstdint>

// The renderer's own Vulkan headers (the C API): including <vulkan/vulkan.h> here pulls in the
// C++ bindings of the SDK's copy, which do not build against this platform's loader.
#include <rex/ui/vulkan/provider.h>

namespace shd::render_scale {

inline constexpr uint32_t kNative = 4;  // q of the game's own 1280x720

// Host edge of a guest pixel edge (any sign).
constexpr int32_t ScaleEdge(int32_t guest, uint32_t q) {
  if (q == kNative) {
    return guest;
  }
  const int64_t n = 2 * int64_t(guest) * int64_t(q) + 3;
  return int32_t(n >= 0 ? n / 8 : -((-n + 7) / 8));
}

constexpr uint32_t ScaleSize(uint32_t guest, uint32_t q) {
  if (q == kNative) {
    return guest;
  }
  const uint32_t scaled = uint32_t(ScaleEdge(int32_t(guest), q));
  return scaled ? scaled : 1u;
}

inline VkRect2D ScaleRect(const VkRect2D& guest, uint32_t q) {
  if (q == kNative) {
    return guest;
  }
  const int32_t x0 = ScaleEdge(guest.offset.x, q);
  const int32_t y0 = ScaleEdge(guest.offset.y, q);
  const int32_t x1 = ScaleEdge(guest.offset.x + int32_t(guest.extent.width), q);
  const int32_t y1 = ScaleEdge(guest.offset.y + int32_t(guest.extent.height), q);
  return VkRect2D{{x0, y0},
                  {uint32_t(x1 > x0 ? x1 - x0 : 0), uint32_t(y1 > y0 ? y1 - y0 : 0)}};
}

// The quarters the cvars ask for, 2 (360p) to 4 (720p). Read once per session.
uint32_t WantedQuarters();
bool Enabled();

}  // namespace shd::render_scale

#endif  // SHD_NATIVE_RENDER_SCALE_H_
