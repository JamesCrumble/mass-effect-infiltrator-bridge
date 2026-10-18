#!/bin/sh
# Waits $1 s (default 0), then prints the last $2 (default 4) bridge stat lines and phase lines.
P=${ME_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}
A=$P/tools/adb
D=/sdcard/BridgeGames/com.jamescrumble.meinfiltrator
[ -n "$($A shell ls $D/meinfiltrator.log 2>/dev/null)" ] || D=/sdcard/Android/data/com.jamescrumble.meinfiltrator/files
F=$D/meinfiltrator.log
[ "${1:-0}" -gt 0 ] && sleep "$1"
$A shell "grep -E 'frames [0-9.]+/s|phase=|qemu exited|died' $F | tail -${2:-4}"
