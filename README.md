# Silent Hill: Downpour for Nintendo Switch

[Русский](README.ru.md)

A native Nintendo Switch port of the **Xbox 360** version of Silent Hill: Downpour (Vatra Games / Konami, 2012), made by
**static recompilation** with [ReXGlue](https://github.com/rexglue/rexglue-sdk): the game's PowerPC code is translated
to C++ and compiled to ARM64 for Horizon OS, and its Direct3D command stream is drawn with Vulkan (NVK/Mesa).
No emulator, no Xbox 360 GPU emulation.

**Free. Not for sale.** News: [t.me/fspores](https://t.me/fspores)

This repository has **no game files**. You need your own copy of the Xbox 360 game.

![status](https://img.shields.io/badge/Silent%20Hill%20Downpour-playable-brightgreen)

## Download

Ready builds (NRO, settings, HOME-menu NSP forwarder): see **Releases**.

## What you get

| | Silent Hill: Downpour |
|---|---|
| Picture | 640x360 + FSR 1 by default (recommended); 960x540 and 1280x720 (the game's own) on the settings screen |
| Status | everything works, 30 fps* |

\* Switch Lite (Mariko) with Horizon OC at CPU 1963 / GPU 1152 / RAM 2333 MHz. Lower at stock clocks.
At 640x360 the console runs at about 57 °C instead of 70 °C at 1280x720.

## Install

Needs Atmosphère 1.8.0+ with sigpatches, DBI (or another NSP installer), about 8 GB on the SD card. FAT32 is fine.

1. Copy `switch/` from the release to the root of the SD card:
   ```
   sdmc:/switch/rex-start.nro
   sdmc:/switch/shd-nx/shd.nro, shd.toml
   ```
2. Put your game files into `sdmc:/switch/shd-nx/game/` (with `default.xex` inside).
   From an ISO: `extract-xiso -x "SilentHillDownpour.iso" -d game`. `$SystemUpdate` is not needed.
3. Install the NSP from `nsp/` with DBI and start the game from the HOME menu. It starts through `rex-start.nro`:
   it shows the game for five seconds, and **+** opens the settings screen (internal resolution).
   Without an NSP: hold **R** while starting any installed game and run `switch/shd-nx/shd.nro` from the Homebrew menu.
   **Not from the Album:** applet mode does not have enough memory.

The default `shd_renderer = "p2"` translates the game's shaders at runtime, so **no shader library is needed**.
The older `shd_renderer = "me"` path can use an offline `shd_shaders.mesp` built from your disc (Building, step 5).
Neither that library nor compiled shaders are shipped: both are made from the game's own shaders. The prewarm file
is not shipped either: besides pipeline keys it contains Xenos microcode. On the first start the game compiles
pipelines in the background from the game's microcode, so the first minutes may stutter. The driver cache and
prewarm list are then generated locally on your card.

### Tested edition

Xbox 360, title ID 4B4E0823, no title update. `default.xex` SHA-256
`c686cbff8facad6fb6b78ed98e5a5a2f4f19337ef83517f56195a56b0db2c8d7`.

The code is recompiled from one specific `default.xex`; other editions may not start.

## Settings

`shd.toml` next to the NRO. Restart the game after a change.

| Key | Default | |
|---|---|---|
| `shd_render_width` x `shd_render_height` | 640x360 | internal resolution: 640x360, 960x540 or 1280x720 |
| `present_effect` | `"fsr"` | `"bilinear"` is slightly faster and softer |
| `p2_native_max_anisotropy` | 16 | 8 or 4 are faster |
| `p2_native_max_mip_levels` | 1 | **leave at 1**: more mip levels bring back square artefacts |
| `user_language` | 1 | 1 English, 12 Russian (depends on your copy) |

With Horizon OC / sys-clk give the game (`0153484400000000`) its own clock profile. Do not use high overclocks on an
Erista (original) console.

## Settings screen (`rex-start/`)

The same small homebrew as in [prototype-switch](https://github.com/fungalspores/prototype-switch): libnx framebuffer,
the console's own font through FreeType, no SDL. For Silent Hill it offers the internal resolution only, writes it
straight into `shd.toml` and copies every other line through untouched. Press nothing and the game starts as before.

## What we fixed

The port is built on the Prototype 2 NX native renderer (the same one as in our Prototype port) with the
masseffect-nx Horizon SDK. Silent Hill is an Unreal Engine 3 game, and it needed its own fixes:

- **Vertex fetch interpreter of the SDK.** In the unsigned branch of the packed formats the components were not
  shifted by `packed_offsets[i]`, so for `k_16_16_16_16` the y and w components were copies of x and z
  (`0x12345678` became `0x56785678`). The signed branch was right, which is why the bug lived on. Prototype 1 and 2
  have it too, it just does not show there.
- **memexport on the CPU.** UE3 moves textures around its pool with a vertex shader that writes memory
  (memexport). The renderer skipped such draws, so every second 8-byte block of the moved textures stayed empty and
  walls and floors came out striped. The shader now runs on the CPU through the SDK interpreter
  (`src/native_p2/native_memexport_cpu.cpp`).
- **Scene copy selection.** Two copies of the HDR scene lived at the same address in different formats. With equal
  frame numbers the renderer took whichever came last in the iteration, the spare one, not the one written last.
  Fixing it removed a blue skew in the lift and brought back the logo reflection in the puddle on the main menu.
- **Stacked resolves.** The game writes 16 colour-grading slices into one row and reads them as a 2D stack; the
  renderer handed back the whole strip as one layer, which made the title screen red and cyan.
- **Internal resolution.** The game's own coordinates are never changed. Render targets keep their guest size in
  every piece of guest logic (EDRAM tiles, resolve rectangles, texture identity by address and size); only the Vulkan
  images and commands are scaled, by quarter steps, and a resolve stretches the smaller image back to the guest
  size. Depth that the game reads back texel by texel is first copied into a full-size image (`DepthFullSize`).
- **Pipeline cache.** Pipelines are compiled on worker threads, so a new material no longer stops the frame; the
  cache is fsync'd before the rename and saved at exit (leaving through HOME used to lose it).
- The SDK fixes from Prototype (saves, console signatures, critical sections) and the speed patches of the
  generated code (`shd/tools/`).

A splash with the credit is shown for 3 seconds before the game (skip with A/B/+).

## Building

Everything was built **on an Android phone** (Alpine Linux in PRoot, aarch64) without Docker; a normal Linux box
works the same way. Steps 1-3 are the same as in
[prototype-switch](https://github.com/fungalspores/prototype-switch#building) (`tools/proot/` is a copy).

1. **devkitA64**: `tools/proot/pull_dka64.py`; on musl hosts `tools/proot/patch_dkp.sh` and `wrap_dkp.sh`.
2. **SDK**: [masseffect-nx](https://github.com/nebadasSwifty/masseffect-nx) at `a85b0b4` +
   `patches/rexglue-sdk-masseffect-nx.patch`, third-party sources with its `tools/fetch_thirdparty.py`, host
   `rexglue` built as in prototype-switch.
3. **Mesa/NVK**: [mesa-switch](https://github.com/danfromtico/mesa-switch) at `d4a00ea0ab3` +
   `patches/mesa-switch.patch`, then `tools/proot/mesa_p2_build.sh`.
4. **Silent Hill**: extract the disc into `game/` next to `shd/`, then `REXGLUE=... REXSDK_DIR=... NVK_SDK=... shd/build.sh`
   (codegen a few minutes, NRO about 35 minutes; `glslangValidator` on the host PATH).
5. **Shader library, only for the legacy `me` renderer** (the default `p2` renderer does not need it):
   `R=<folder with game/> ME=<masseffect-nx at 650e14e + patches/masseffect-nx-xenosrecomp.patch>
   REXSDK=<patched SDK> DXC=<DXC built for the host> sh shd/tools/build_shaders.sh` (about 25 minutes). It pulls the
   15 702 shader containers out of the UE3 packages, turns them into HLSL with XenosRecomp, compiles SPIR-V with DXC
   and packs `shd_shaders.mesp` + `.idx`.
6. **Settings screen**: `rex-start/build.sh` (devkitA64 + FreeType from the portlibs).
7. **NSP forwarder**: `forwarder/build_forwarder.py --launcher sdmc:/switch/rex-start.nro --nro sdmc:/switch/shd-nx/shd.nro --icon art/icon_shd.jpg --title "Silent Hill Downpour" --author fungalspores --version 0.2.0 --tid 0153484400000000 --keys <your prod.keys> --out out/`
   (needs [Forwarder-Mod](https://github.com/Skywalker25/Forwarder-Mod) `9853bd9` cloned to `forwarder/loader`).
   Your own `prod.keys` only; never publish them.

`docs/D3D_SHD.md` is the map of the game's XDK Direct3D objects (in Russian).

## Reporting

Logs are written to `switch/shd-nx/logs/`. A log of the session plus a screenshot is what makes a defect findable.
Telegram: [t.me/fspores](https://t.me/fspores)

## Credits

- **penettrator**: [prototype2-nx](https://github.com/penettrator/prototype2-nx), the native renderer this port draws with.
- **NebadasSwifty**: [masseffect-nx](https://github.com/nebadasSwifty/masseffect-nx), the SDK with the Horizon layer
  and the shader pipeline (XenosRecomp, the `.mesp` library).
- **StevensND**: the Horizon layer of ReXGlue and [nfsmw-nx](https://github.com/StevensND/nfsmw-nx).
- **R-drg**: [project8-sw](https://github.com/R-drg/project8-sw), the base of the native renderer.
- **DownpourRecomp**: addresses and the colour-grading path, which we checked our findings against.
- **Tom Clay** and the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk); the [Xenia](https://xenia.jp) project.
- **danfromtico**: [mesa-switch](https://github.com/danfromtico/mesa-switch); the [Mesa](https://mesa3d.org) project.
- **Skywalker25**: [Forwarder-Mod](https://github.com/Skywalker25/Forwarder-Mod); switchbrew (libnx); devkitPro.
- AMD FidelityFX Super Resolution 1.

## License

GPL-3.0 (see `LICENSE`); third-party components under their own licenses. Silent Hill is a trademark of Konami.
This project is not affiliated with Konami, Vatra Games, Microsoft or Nintendo and contains none of their files.
