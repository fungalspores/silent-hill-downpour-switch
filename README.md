# Silent Hill: Downpour for Nintendo Switch

A native Switch port of the **Xbox 360** version of Silent Hill: Downpour (Vatra Games / Konami, 2012),
built with [ReXGlue](https://github.com/fungalspores/prototype-switch) — the same toolchain as the
Prototype ports. The Xbox 360 code is recompiled to native ARM code; the graphics run on a native
Vulkan (NVK) renderer instead of emulating the Xenos GPU.

**This is an early test build (0.1.0).** The game boots and plays from the start to the forest, but
there are visual defects. It is published so people can try it and send reports, not as a finished port.

## Status

Works:

* Intros, menu, brightness screen
* Prison, lift, corridors, the street, the forest
* Sound, music, FMV
* Saves
* Mostly 30 fps

Broken:

* Some wall and floor textures come out smeared or striped
* The loading screen shows a multilingual mess instead of text
* FMV sometimes has noise and garbage
* Occasional stalls when new effects first appear (up to 1-2 s)
* Nothing past the forest is tested

## What you need

* A Nintendo Switch with Atmosphère 1.8.0 or newer
* Your own copy of Silent Hill: Downpour for Xbox 360 (ISO or an extracted folder with `default.xex`).
  The code is recompiled against one specific `default.xex`; other releases may not boot
* About 8 GB on the card. FAT32 is fine
* Overclock (Horizon OC or sys-clk). Without it the frame rate is not playable

Reference console: Switch Lite (Mariko) at CPU 1963 / GPU 1152 / RAM 2333 MHz, around 70 °C under load.

## Install

1. Copy the `switch` folder from a release to the root of the SD card
2. Extract your game into `switch/shd-nx/game/` so that `default.xex` sits inside.
   From an ISO: `extract-xiso -x "SilentHillDownpour.iso" -d game`. `$SystemUpdate` is not needed
3. Launch `switch/shd-nx/shd.nro` from the homebrew menu

Do not launch through the Album — the game does not get enough memory in that mode.

## How it works

* The Xbox 360 executable is statically recompiled to native ARM64 (ReXGlue)
* Graphics go through a native Vulkan renderer, not GPU emulation, so there is sound and the menus run
  at 30 fps
* All 15 702 game shaders are translated to SPIR-V ahead of time and shipped as a library next to the game

### Fixes made for this port

* **SDK vertex fetch interpreter**: in the unsigned branch of the packed formats the components were
  not shifted by `packed_offsets[i]`, so for `k_16_16_16_16` the y and w components were copies of x
  and z (`0x12345678` became `0x56785678`). The signed branch shifted correctly, which is why the bug
  survived — it also affects Prototype 1 and 2
* **Scene copy selection**: with two copies of the HDR scene at the same address and the same frame
  number, the renderer picked whichever came last in the iteration — the spare one, not the one written
  last. Fixing it removed a blue-channel skew in the lift and brought back the logo reflection in the
  puddle on the main menu
* **Stacked resolves**: the game writes 16 colour-grading slices into one row and reads them as a
  2D stack; the renderer handed back the whole strip as a single layer, which made the title screen
  red and cyan

## Settings

`switch/shd-nx/shd.toml`, next to the NRO. Close the game fully after editing.

| Key | Meaning |
|---|---|
| `user_language` | 1 = English, 12 = Russian (depends on your copy) |
| `present_effect` | `"fsr"` or `"bilinear"` (slightly faster, softer) |
| `p2_native_max_anisotropy` | 16 by default; 8 or 4 are faster |
| `p2_native_max_mip_levels` | **leave at 1** — mip levels bring back square artefacts |

## Reporting

Logs are written to `switch/shd-nx/logs/`. A log of the session plus a screenshot is what makes a
defect findable.

Telegram: [t.me/fspores](https://t.me/fspores)

## Licence

See `LICENSE`. The game itself is not distributed — you need your own copy.
