#!/bin/sh
# Silent Hill: Downpour — rebuild the shader library from the game files.
#   sh tools/build_shaders.sh          full run (scan -> HLSL -> SPIR-V -> package), about 25 minutes
# Output: shaders/lib/shd_shaders.mesp + .idx
set -e
R=/root/shd
ME=$R/ref/masseffect-nx
X=/root/proto/menx/sdk/thirdparty/xxHash
F=/root/proto/menx/sdk/thirdparty/fmt/include
M=/root/proto/menx/sdk/thirdparty/libmspack/libmspack/mspack

echo "== tools"
[ -x $R/tools/ue3_shader_scan ] || {
  gcc -O2 -c -I$M $M/lzxd.c -o /tmp/lzxd.o
  g++ -std=c++20 -O2 -I$M $R/tools/ue3_shader_scan.cpp $R/tools/ue3_lzx.cpp /tmp/lzxd.o -o $R/tools/ue3_shader_scan
}
[ -x $ME/out/tools/xenos_hlsl ] || (cd $ME && XXHASH_DIR=$X FMT_DIR=$F sh shaders/tools/build_xenos_hlsl.sh)
[ -x $ME/out/tools/me_pack_shaders ] || g++ -std=c++20 -O2 -I$X \
  $ME/shaders/tools/me_pack_shaders.cpp $ME/app/src/native/masseffect/masseffect_shader_library.cpp \
  -o $ME/out/tools/me_pack_shaders

echo "== 1. containers out of the UE3 packages (LZX)"
rm -rf $R/shaders/containers && mkdir -p $R/shaders/containers
$R/tools/ue3_shader_scan $R/shaders/containers \
  $R/game/SHGame/CookedXenon/GlobalShaderCache-Xbox360.bin \
  $R/game/SHGame/CookedXenon/*.upk $R/game/SHGame/CookedXenon/*.xxx | tail -2

echo "== 2. HLSL (XenosRecomp + the SHD changes)"
rm -rf $R/shaders/hlsl
$ME/out/tools/xenos_hlsl $R/shaders/containers $R/shaders/hlsl $R/shaders/shd_shader_common.h 2>/dev/null | tail -1

echo "== 3. SPIR-V (DXC built for aarch64 in tools/dxc-build)"
rm -rf $R/shaders/spv && mkdir -p $R/shaders/spv
ls $R/shaders/hlsl/*.hlsl | OUT=$R/shaders/spv xargs -n1 -P4 $R/tools/dxc_one.sh
echo "compiled $(wc -l < $R/shaders/spv/ok.txt), failed $(cat $R/shaders/spv/cfail.txt $R/shaders/spv/valfail.txt 2>/dev/null | wc -l)"

echo "== 4. package"
rm -rf $R/shaders/lib && mkdir -p $R/shaders/lib
$ME/out/tools/me_pack_shaders $R/shaders/containers $R/shaders/spv $R/shaders/lib/shd_shaders.mesp | tail -2
