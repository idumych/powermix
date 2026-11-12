#!/bin/sh
# haiku-run.sh [ARGS...]   sync the sources to the Haiku test VM, build,
# restart PowerMix with ARGS (log in /tmp/pm.log there). Takes the shared
# VM lock for the duration. SHOT=name also saves a screenshot after W s.
L=/tmp/haiku-b6-test.vm.lock
until mkdir $L 2>/dev/null; do sleep 15; done
trap 'rmdir $L' EXIT
cd "$(dirname "$0")/.."
tar -cf - Makefile PowerMix.rdef src | ssh haiku-b6-test 'mkdir -p /boot/home/powermix && tar -m -C /boot/home/powermix -xf -'
ssh haiku-b6-test 'cd /boot/home/powermix && make 2>&1 | grep -E "error|Error" ; kill PowerMix 2>/dev/null; sleep 1'
ssh haiku-b6-test "cd /boot/home/powermix && (./PowerMix $* > /tmp/pm.log 2>&1 &); sleep ${W:-4}; cat /tmp/pm.log"
if [ -n "$SHOT" ]; then
	VBoxManage controlvm haiku-b6-test screenshotpng /tmp/claude-1000/-home-ian-Projects-powermix/84e01c99-054e-457c-a53b-d8df8d0a3bd5/scratchpad/$SHOT.png
fi
