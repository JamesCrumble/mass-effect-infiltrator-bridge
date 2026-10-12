#!/bin/sh
# Installs the built APK (if "install" given), force-stops and starts the game.
P=${ME_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}
A=$P/tools/adb
if [ "${1:-}" = install ]; then
  $A install -r "$(wslpath -w $P/MassEffect-Infiltrator-debug.apk)" || exit 1
fi
$A shell am force-stop com.jamescrumble.meinfiltrator
$A shell am start -n com.jamescrumble.meinfiltrator/.MainActivity
