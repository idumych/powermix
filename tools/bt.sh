#!/bin/sh
# bt.sh 'command' [shot-name]   type a command + Enter into the focused
# window of the BeOS VM ($VM, default BeOS-3dmix); optional screenshot.
VM=${VM:-BeOS-3dmix}
S=${S:-/tmp/claude-1000/-home-ian-Projects-powermix/84e01c99-054e-457c-a53b-d8df8d0a3bd5/scratchpad}
VBoxManage controlvm "$VM" keyboardputstring "$1"
VBoxManage controlvm "$VM" keyboardputscancode 1c 9c
if [ -n "$2" ]; then sleep "${W:-2}"; VBoxManage controlvm "$VM" screenshotpng "$S/$2.png"; fi
