#!/bin/sh
# Copies the phone's game log to ~/mass-effect-infiltrator-probe/logs/phone.log
P=${ME_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}
A="$P/tools/adb"
D=/sdcard/BridgeGames/com.jamescrumble.meinfiltrator
[ -n "$($A shell ls $D/meinfiltrator.log 2>/dev/null)" ] || D=/sdcard/Android/data/com.jamescrumble.meinfiltrator/files
$A shell cat $D/meinfiltrator.log | tr -d '\r' > "$P/logs/phone.log"
wc -l "$P/logs/phone.log"
