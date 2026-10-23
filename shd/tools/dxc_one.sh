#!/bin/sh
f=$1; b=$(basename "$f" .hlsl); OUT=${OUT:-/tmp/dxc_rand}
case $b in ps_*) t=ps_6_6; x="";; *) t=vs_6_6; x=-fvk-invert-y;; esac
if "${DXC:?}/bin/dxc" -spirv -T $t -E main -HV 2021 \
     -fspv-target-env=vulkan1.2 -fvk-use-dx-layout -Werror=parameter-usage $x \
     -Fo "$OUT/$b.spv" "$f" > "$OUT/$b.cerr" 2>&1; then
  if spirv-val --target-env vulkan1.2 --scalar-block-layout "$OUT/$b.spv" >> "$OUT/$b.cerr" 2>&1; then
    rm -f "$OUT/$b.cerr"; echo "OK $b" >> "$OUT/ok.txt"
  else
    echo "VALFAIL $b" >> "$OUT/valfail.txt"
  fi
else
  echo "CFAIL $b" >> "$OUT/cfail.txt"
fi
