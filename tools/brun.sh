#!/bin/sh
# brun.sh 'command'   run a shell command in the BeOS VM's focused Terminal
# and print its output here. The output comes back through the raw transfer
# disk (see xfer-out.sh); the Terminal must have focus. T= timeout (s).
VM=${VM:-BeOS-3dmix}
H=$(dirname "$0")
O=$(mktemp -d)
TOK=BRUN$$x$(date +%s)
VBoxManage controlvm "$VM" keyboardputstring "clear; ($1) > /tmp/o.txt 2>&1; echo $TOK >> /tmp/o.txt; cd /tmp; tar cf /dev/disk/ide/ata/0/slave/0/raw o.txt; sync; cd; echo BRUN-DONE"
VBoxManage controlvm "$VM" keyboardputscancode 1c 9c
i=0
while [ $i -lt "${T:-60}" ]; do
	sleep 1; i=$((i + 1))
	rm -f "$O/o.txt"; "$H/xfer-out.sh" "$O" 2>/dev/null
	grep -aq "$TOK" "$O/o.txt" 2>/dev/null && break
done
grep -av "$TOK" "$O/o.txt" 2>/dev/null || echo "brun: no output after ${T:-60}s"
rm -rf "$O"
