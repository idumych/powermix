#!/bin/sh
# btype.sh LOCAL GUESTPATH   type a small text file into the BeOS VM's
# focused Terminal as a heredoc (tabs become spaces; no base64 on BeOS 5).
VM=${VM:-BeOS-3dmix}
VBoxManage controlvm "$VM" keyboardputstring "cat > $2 <<'EOFX'"
VBoxManage controlvm "$VM" keyboardputscancode 1c 9c
expand -t4 "$1" | while IFS= read -r line; do
	[ -n "$line" ] && VBoxManage controlvm "$VM" keyboardputstring "$line"
	VBoxManage controlvm "$VM" keyboardputscancode 1c 9c
done
VBoxManage controlvm "$VM" keyboardputstring "EOFX"
VBoxManage controlvm "$VM" keyboardputscancode 1c 9c
