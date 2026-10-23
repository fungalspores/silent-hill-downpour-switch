#include "native_render_scale.h"

#include <algorithm>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_UINT32(shd_scale_pitches, 15, "SilentHill",
                      "Diagnostic: which target pitches follow shd_render_width. Bit 0 = 1280 "
                      "(scene), 1 = 1040 (AuxColor), 2 = 640 (half-size), 3 = 320 (quarter). "
                      "15 = all; restart required")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(shd_scale_msaa, false, "SilentHill",
                    "Scale surfaces drawn with their MSAA samples as pixels too. Off: they keep the "
                    "guest size (diagnostic for the scene repeat at 960); restart required")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(shd_diag_scale, false, "SilentHill",
                    "Diagnostics of the internal resolution: [shd-scale] / [shd-vp] lines in the log "
                    "(targets, passes, resolve paths); off in release builds")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DECLARE(uint32_t, shd_render_width);
REXCVAR_DECLARE(uint32_t, shd_render_height);

namespace shd::render_scale {
namespace {
constexpr uint32_t kGameWidth = 1280;
}  // namespace

// Quarters from the asked width: 1280 -> 4 (the game's own), 960 -> 3, 640 -> 2. The height
// follows the width, as the frame is 16:9 either way; a width between two quarters goes to the
// nearer one, so the picture never stretches.
uint32_t WantedQuarters() {
  static const uint32_t q = [] {
    const uint32_t width = std::clamp<uint32_t>(REXCVAR_GET(shd_render_width), 640, kGameWidth);
    uint32_t quarters =
        std::clamp<uint32_t>((width * kNative + kGameWidth / 2) / kGameWidth, 2, kNative);
    // Only halves and full size. The engine draws the frame through targets of several pitches
    // (1280, 1040, 880, 640 in the forest) and three quarters of 1040 or 880 is not a whole
    // 8-pixel group: those targets came out a pixel off from the ones that divided evenly, and
    // the seams showed as noise along every edge. A half is exact for all of them.
    // 3/4 (960x540) is allowed again: it was forced to 1/2 on a guess about pixel alignment that
    // was never confirmed -- the edge noise of that night came from the depth paths, since fixed.
    if (quarters != kNative) {
      REXLOG_INFO("[shd-quality] internal render scale {}/4: {}x{} (game 1280x720)", quarters,
                  ScaleSize(kGameWidth, quarters), ScaleSize(720, quarters));
    }
    return quarters;
  }();
  return q;
}

bool Enabled() { return WantedQuarters() != kNative; }

}  // namespace shd::render_scale
