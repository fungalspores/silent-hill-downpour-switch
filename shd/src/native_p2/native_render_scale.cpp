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
REXCVAR_DEFINE_BOOL(shd_ui_native, false, "SilentHill",
                    "Sharper Scaleform text: after the first glyph draw of a frame, render the "
                    "1040-pitch text target and its 1280-pitch composition at guest-native size; "
                    "3D and earlier post-process still follow shd_render_width; restart required")
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

// Sixteenths from the asked width: 1280 -> 16 (the game's own), 960 -> 12, 720 -> 9,
// 640 -> 8. The height follows the width, as the frame is 16:9 either way; intermediate 80-pixel
// steps are exact and do not stretch the picture.
uint32_t WantedScale16() {
  static const uint32_t q = [] {
    const uint32_t width = std::clamp<uint32_t>(REXCVAR_GET(shd_render_width), 640, kGameWidth);
    const uint32_t sixteenths =
        std::clamp<uint32_t>((width * kNative + kGameWidth / 2) / kGameWidth, 8, kNative);
    // 3/4 (960x540) was once disabled on a guess about pixel alignment that was never confirmed;
    // the edge noise of that night came from the depth paths, since fixed. Keep all sixteenths
    // available, with the old presets using exactly their former edge arithmetic.
    if (sixteenths != kNative) {
      REXLOG_INFO("[shd-quality] internal render scale {}/16: {}x{} (game 1280x720)", sixteenths,
                  ScaleSize(kGameWidth, sixteenths), ScaleSize(720, sixteenths));
    }
    return sixteenths;
  }();
  return q;
}

bool Enabled() { return WantedScale16() != kNative; }

}  // namespace shd::render_scale
