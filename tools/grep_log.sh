#!/bin/sh
# grep -E "$1" over the phone's game log (last $2 lines, default 40).
P=${ME_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}
A=$P/tools/adb
D=/sdcard/BridgeGames/com.jamescrumble.meinfiltrator
[ -n "$($A shell ls $D/meinfiltrator.log 2>/dev/null)" ] || D=/sdcard/Android/data/com.jamescrumble.meinfiltrator/files
F=$D/meinfiltrator.log
$A shell "grep -aE '$1' $F | tail -${2:-40}"
