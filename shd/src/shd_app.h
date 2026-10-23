// Silent Hill: Downpour (Xbox 360, 4B4E0823) for Nintendo Switch - ReXGlue recompiled app.
// Structure after penettrator/prototype2-nx project/src/prototype2_app.h (GPL-3.0).
#pragma once

#include "native_p2/p2_native_system.h"
#include <filesystem>
#include <string>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/rex_app.h>
#include <rex/runtime.h>
#include <rex/system/kernel_state.h>

#include "native/me_native_system.h"

REXCVAR_DECLARE(std::string, render_target_path_vulkan);
REXCVAR_DECLARE(std::string, shd_renderer);
void proto_ApplySpeedSettings();  // proto_speed.cpp
extern "C" void ShdApplyGamePatches(uint8_t* base);  // shd_game_patches.cpp

class ShdApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<ShdApp>(new ShdApp(ctx, "shd", PPCImageConfig));
  }

  // Without a gpu_plugin the Vd* kernel calls are ignored and nothing renders.
  void OnPostInitLogging() override {
    if (!rex::cvar::HasNonDefaultValue("gpu_plugin")) {
      rex::cvar::SetFlagByName("gpu_plugin", "xenos");
    }
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    proto_ApplySpeedSettings();
    // p2_renderer = "native" (prototype.toml): the game's Direct3D ring is drawn straight with Vulkan and
    // the SDK's Xenos emulation, with its watched pages and emulated reads, is not created.
    // shd_renderer = "me" (EDRAM mode 4, the Mass Effect NX renderer) or "p2" (the Prototype 2 one).
    // Both are built in: one NRO, the toml picks. "xenos" falls through to the SDK emulation.
    const std::string which = REXCVAR_GET(shd_renderer);
    if (which == "p2" && p2::native::Enabled()) {
      config.graphics = p2::native::CreateGraphicsSystem();
      REXLOG_INFO("SHD Switch renderer: native (Prototype 2 renderer)");
    } else if (which != "p2" && me::native::Enabled()) {
      config.graphics = me::native::CreateGraphicsSystem();
      REXLOG_INFO("SHD Switch renderer: native (Mass Effect NX renderer, EDRAM mode 4)");
    } else {
      REXLOG_INFO("SHD Switch renderer: SDK Xenos emulation");
    }
    if (rex::cvar::GetFlagSource("render_target_path_vulkan") == rex::cvar::Source::kDefault &&
        REXCVAR_GET(render_target_path_vulkan).empty()) {
      REXCVAR_SET(render_target_path_vulkan, std::string("fsi"));
    }
  }

  // The extracted disc lives in game/ next to the NRO (sdmc:/switch/shd-nx/game).
  // The guest image is mapped by now and the game has not run yet: patch its code here.
  void OnPreLaunchModule() override {
    ShdApplyGamePatches(REX_KERNEL_MEMORY()->virtual_membase());
  }

  void OnConfigurePaths(rex::PathConfig& paths) override {
    if (!paths.game_data_root.empty()) return;
    std::error_code ec;
    for (const char* name : {"game", "game_root"}) {
      const auto dir = rex::filesystem::GetExecutableFolder() / name;
      if (std::filesystem::is_directory(dir, ec)) {
        paths.game_data_root = dir;
        return;
      }
    }
    // Some NRO loaders (forwarders) give no argv[0].
    for (const char* dir : {"/switch/shd-nx/game", "/switch/SilentHillDownpour/game"}) {
      ec.clear();
      if (std::filesystem::is_directory(dir, ec)) {
        paths.game_data_root = dir;
        return;
      }
    }
  }
};
