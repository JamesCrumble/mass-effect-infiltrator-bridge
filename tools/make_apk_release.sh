#!/bin/bash
# Builds an Android release bundle in release/<version>/ (git-ignored):
#   MassEffect-Bridge-<v>.apk      release-signed APK (not debuggable)
#   MassEffect-Bridge-<v>-src.zip  source of the committed HEAD
#   INSTALL.txt, SHA256SUMS
# The game's own files are the player's and are never part of a release.
# (tools/make_release.sh is the PortMaster package, a different thing.)
# Usage: tools/make_apk_release.sh   (run from WSL; commit first - the source zip is HEAD)
set -euo pipefail
P=${ME_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}
source "$P/env.sh"
cd "$P"
Q="$P/qemu/out/qemu-arm-${ME_QEMU_VARIANT:-F}.stripped"
VER=$(grep -o 'versionName="[^"]*"' apk/AndroidManifest.xml | head -1 | cut -d'"' -f2)
OUT="$P/release/$VER"
rm -rf "$OUT"; mkdir -p "$OUT"
"$P/build.sh" > logs/build.log 2>&1
QEMU_BIN="$Q" RELEASE=1 bash apk/build_apk.sh > logs/apk_release.log 2>&1
tail -2 logs/apk_release.log
mv "MassEffect-Bridge-$VER.apk" "$OUT/"
rm -f "MassEffect-Bridge-$VER.apk.idsig"
git archive --format=zip --prefix="MassEffect-Bridge-$VER/" -o "$OUT/MassEffect-Bridge-$VER-src.zip" HEAD
cp tools/release_install.txt "$OUT/INSTALL.txt"
(cd "$OUT" && sha256sum * > SHA256SUMS)
ls -la "$OUT"
