// Silent Hill: Downpour — the settings the ported Mass Effect renderer reads but whose owners are game
// specific modules that were not carried over (internal resolution of the scene viewport, audio pacing).
// Keeping them here with the Mass Effect defaults means the renderer behaves as "feature off".
#include <rex/cvar.h>

// me_resolution.cpp in Mass Effect: it renders the scene smaller than the back buffer and scales UE3's
// thresholds. Downpour has no such path yet, so the scene size is "as the game draws it" (0 = unchanged).
REXCVAR_DEFINE_INT32(shd_scene_width, 0, "Silent Hill",
                     "Internal width of the scene viewport (0 = as the game draws it; not wired up yet)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_INT32(shd_scene_height, 0, "Silent Hill",
                     "Internal height of the scene viewport (0 = as the game draws it; not wired up yet)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_INT32(shd_scene_parts, 0xFF, "Silent Hill",
                     "Which passes follow the internal scene size (bit 0 = the scene itself)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

// Diagnostic, used by me_native_system.cpp (see there). No image change.
REXCVAR_DEFINE_INT32(shd_diag_draw_sequence, 0, "Silent Hill",
                     "Diagnostics: log every draw in order ([draw-seq] line per draw: number, color/depth/tex0 base, "
                     "formats) for N draws. 0 = off. Window starts at shd_diag_draw_sequence_from. No image change")
    .range(0, 1000000);
REXCVAR_DEFINE_INT32(shd_diag_draw_sequence_from, 0, "Silent Hill",
                     "Diagnostics: first draw number of the shd_diag_draw_sequence window (0 = from the start of the run)")
    .range(0, 100000000);
REXCVAR_DEFINE_INT32(shd_diag_draw_sequence_every, 0, "Silent Hill",
                     "Diagnostics: log a [draw-seq] line for every N-th draw of the whole run (0 = off). Independent of "
                     "shd_diag_draw_sequence; keeps the whole run in the log at a fixed rate. No image change")
    .range(0, 1000000);
REXCVAR_DEFINE_BOOL(shd_diag_draw_targets, false, "Silent Hill",
                    "Diagnostics: for every distinct combination of color target, depth target and texture 0 base "
                    "seen by a draw, log one line [draw-target] with the draw's sequence number. Shows which "
                    "engine buffer (by EDRAM base) is drawn into and which is read. No image change");

// me_audio_hooks.cpp in Mass Effect: how the vblank thread waits.
REXCVAR_DEFINE_BOOL(shd_vblank_sleep_exact, true, "Silent Hill",
                    "The vblank thread sleeps to the exact next vblank instead of a fixed interval");

// --- entry points of the Mass Effect modules this port does not carry over --------------------------------
#include <cstdint>

// me_resolution.cpp: Mass Effect can scale the back buffer and show only a corner of the front buffer.
// Downpour draws at its own size, so the output is the whole front buffer.
extern "C" bool MeResolutionOutputSize(uint32_t*, uint32_t*) { return false; }

namespace me::native {
// me_hang_watchdog.cpp: a watchdog that resolves hangs by inspecting guest threads. Not ported yet.
void HangWatchdogInterval(uint64_t, uint64_t) {}
// me_ring_wait.cpp: wakes the game's spinning Direct3D waits as soon as the ring makes progress.
void NotifyRingProgress() {}
}  // namespace me::native
