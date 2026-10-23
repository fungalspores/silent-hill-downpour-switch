// Silent Hill: Downpour — internal render resolution (shd_render_width / shd_render_height).
//
// UE3 builds every buffer of the frame through RHICreateTargetableSurface
// (sub_829CC6D8: sret r3, width r4, height r5, pixel format r6, texture r7, name r8), with the
// scene buffers created at the game's own 1280x720. Scaling the width and height here makes the
// engine render into smaller targets; the presenter scales the result to the screen
// (present_effect = "fsr"). Half- and quarter-size effect buffers are derived by the engine from
// the same call, so they follow automatically.
//
// Only the full-size scene buffers are scaled. Shadow maps and other fixed-size buffers
// (1024x1024, 512x512...) keep their size: they are not tied to the screen, and shrinking them
// costs shadow quality rather than pixels.
#include <algorithm>
#include <cstdint>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_UINT32(shd_render_width, 1280, "SilentHill",
                      "Internal render width (the game's own 1280). Lower = fewer pixels for the "
                      "GPU; the picture is scaled to the screen (see present_effect). Multiple of "
                      "80 (the Xbox 360 EDRAM tile grid), 640..1280; restart required")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_UINT32(shd_render_height, 720, "SilentHill",
                      "Internal render height (the game's own 720). Multiple of 16, 360..720; "
                      "restart required")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace {
// The game's own size, the one the scene buffers are created at.
constexpr uint32_t kGameWidth = 1280, kGameHeight = 720;

uint32_t ScaledWidth() {
  return std::clamp<uint32_t>(REXCVAR_GET(shd_render_width), 640, kGameWidth) / 80 * 80;
}
uint32_t ScaledHeight() {
  return std::clamp<uint32_t>(REXCVAR_GET(shd_render_height), 360, kGameHeight) / 16 * 16;
}
}  // namespace

bool ShdRenderScaleEnabled() {
  return ScaledWidth() != kGameWidth || ScaledHeight() != kGameHeight;
}

// Returns true when the size was changed.
//
// DISABLED (log shd_145): scaling the engine's scene buffers alone leaves the frame the game
// presents at its own 1280x720 -- that buffer is not created through SceneRenderTargets, so it
// never passed this hook. The engine then drew its smaller scene into the corner of a full-size
// frame: a small picture in the top-left of the screen, with the rest black.
//
// The frame has to be scaled where it is presented instead, which is the renderer's business, not
// the engine's. Kept as the place the decision is made, so the key still exists and does nothing
// until the presenter side is done.
bool ShdScaleSurfaceSize(uint32_t& w, uint32_t& h) {
  (void)w;
  (void)h;
  return false;
}

void ShdLogRenderScaleOnce() {
  static bool logged = false;
  if (logged) {
    return;
  }
  logged = true;
  if (ShdRenderScaleEnabled()) {
    REXLOG_INFO("[shd-quality] internal render size {}x{} (game {}x{})", ScaledWidth(),
                ScaledHeight(), kGameWidth, kGameHeight);
  }
}
