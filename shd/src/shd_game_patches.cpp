// Silent Hill: Downpour — patches on the game's own code, each behind a cvar.
//
// "Disable FXAA & MLAA" comes from the PC port's xenia_patches.toml (author: boma). Its post-process
// anti-aliasing runs over the whole screen and smears everything, including the chromatic noise we are
// hunting: with it off a defect is seen where it is produced instead of spread over the picture. It is also
// the single most useful A/B switch for "is this a post-process problem or not".
//
// The addresses were verified against our base XEX (the PC port patches the TU1 build, but these three words
// are identical here): 0x82A04BA0 = 807FAEAC, 0x82A10964 = 817CAEA0.
#include <cstdint>
#include <cstring>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(shd_disable_post_aa, false, "Silent Hill",
                    "Disable the game's own post-process anti-aliasing (SHGame.MLAAPostProcess and "
                    "Engine.GlobalPostProcessSettings.ForceFXAA). Sharper picture, and defects are seen where "
                    "they are produced instead of smeared over the screen")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace {
void Store32(uint8_t* base, uint32_t address, uint32_t value) {
  const uint32_t swapped = __builtin_bswap32(value);
  std::memcpy(base + address, &swapped, sizeof(swapped));
}
void Store16(uint8_t* base, uint32_t address, uint16_t value) {
  const uint16_t swapped = __builtin_bswap16(value);
  std::memcpy(base + address, &swapped, sizeof(swapped));
}
}  // namespace

// Applied from the app (shd_app.h) once the guest image is mapped and before the game runs.
extern "C" void ShdApplyGamePatches(uint8_t* base) {
  if (!REXCVAR_GET(shd_disable_post_aa)) return;
  Store32(base, 0x82A04BA0, 0x38600000);  // li r3, 0 : MLAAPostProcess off
  Store16(base, 0x82A1095F + 1, 0x6441);  // cmpwi r8, 0x64; beq
  Store32(base, 0x82A10964, 0x39600000);  // li r11, 0 : ForceFXAA off
  REXLOG_INFO("[shd] the game's post-process anti-aliasing (MLAA + FXAA) is off");
}
