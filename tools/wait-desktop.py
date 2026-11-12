#!/usr/bin/env python3
"""wait-desktop.py [TIMEOUT]  wait until the BeOS VM ($VM) shows the
Deskbar's red 'OS' of the Be logo at the top right (1024x768 desktop)."""
import io, os, subprocess, sys, time
from PIL import Image
vm = os.environ.get("VM", "BeOS-3dmix")
end = time.time() + float(sys.argv[1] if len(sys.argv) > 1 else 120)
while time.time() < end:
    p = subprocess.run(["VBoxManage", "controlvm", vm, "screenshotpng", "/dev/stdout"],
                       capture_output=True)
    try:
        im = Image.open(io.BytesIO(p.stdout)).convert("RGB")
        if im.size == (1024, 768):
            reds = sum(1 for x in range(966, 985) for y in range(4, 18)
                       if (lambda c: c[0] > 180 and c[1] < 80 and c[2] < 80)(im.getpixel((x, y))))
            if reds > 30:
                time.sleep(3); print("desktop up"); sys.exit(0)
    except Exception:
        pass
    time.sleep(2)
print("timeout"); sys.exit(1)
