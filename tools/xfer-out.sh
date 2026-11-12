#!/bin/sh
# xfer-out.sh DEST   unpack the tar that the BeOS guest wrote to its raw
# transfer disk (vm/beos/xfer.vdi, IDE primary slave) into DEST.
# In the guest:  tar cf /dev/disk/ide/ata/0/slave/0/raw <files>; sync
# Run with the VM powered off or paused (or at least after the guest's sync).
# A fixed VDI is linear after its header; offsetData is the uint32 at 0x158.
set -e
IMG=$(dirname "$0")/../vm/beos/xfer.vdi
OFF=$(od -An -tu4 -j 344 -N4 "$IMG" | tr -d ' ')
mkdir -p "$1"
tail -c +$((OFF + 1)) "$IMG" | tar -xf - -C "$1"
