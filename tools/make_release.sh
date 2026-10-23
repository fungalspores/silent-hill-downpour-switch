#!/bin/sh
# Builds the release folder: NRO + toml + shader cache, rex-start, NSP forwarder, README, licenses.
# No game data, no shader library built from the disc, no keys.
#   VER=0.2.0 NRO=<shd.nro> TOML=<release shd.toml> CACHE=<shd/cache folder> REXSTART=<rex-start.nro>
#   NSP=<forwarder nsp> OUT=<output folder> sh tools/make_release.sh
set -e
G=$(cd "$(dirname "$0")/.." && pwd)
: "${VER:?}" "${NRO:?}" "${TOML:?}" "${CACHE:?}" "${REXSTART:?}" "${NSP:?}" "${OUT:?}"
D="$OUT/Silent-Hill-Downpour-Switch-$VER"
rm -rf "$D"
mkdir -p "$D/switch/shd-nx/shd" "$D/nsp" "$D/licenses"
cp "$NRO" "$D/switch/shd-nx/shd.nro"
cp "$TOML" "$D/switch/shd-nx/shd.toml"
cp -r "$CACHE" "$D/switch/shd-nx/shd/cache"
find "$D/switch/shd-nx/shd/cache" -name '*.tmp' -delete
cp "$REXSTART" "$D/switch/rex-start.nro"
cp "$NSP" "$D/nsp/Silent Hill Downpour [0153484400000000][v0].nsp"
cp "$G/release/README.txt" "$D/README.txt"
cp "$G/LICENSE" "$D/LICENSE"
cp "$G/art/icon_shd.jpg" "$D/icon_silenthill.jpg"
cp "$G/forwarder/LICENSE-Forwarder-Mod.md" "$D/licenses/LICENSE-Forwarder-Mod-nx-hbloader.md"
# Guard: no game data, no shader library, no console keys.
if find "$D" -iname '*.xex' -o -iname '*.upk' -o -iname '*.xxx' -o -iname '*.mesp*' -o -iname 'prod.keys' -o -iname 'title.keys' | grep -q .; then
  echo "FORBIDDEN FILES IN RELEASE"; exit 1
fi
cd "$D" && find . -type f -not -name SHA256SUMS.txt | sort | while IFS= read -r f; do sha256sum "$f"; done > SHA256SUMS.txt
cd "$OUT" && rm -f "Silent-Hill-Downpour-Switch-$VER.zip" && zip -qr "Silent-Hill-Downpour-Switch-$VER.zip" "Silent-Hill-Downpour-Switch-$VER"
du -sh "$D" "$OUT/Silent-Hill-Downpour-Switch-$VER.zip"
