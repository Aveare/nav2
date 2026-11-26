"""Host C syntax check of changed production files; not an ARM compile/link."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
includes = [root/'apps/common/app',root/'config/sentry_2026']
for name in ['Drivers','Middlewares','apps/sentry','Inc','framework','ports','components']:
    parent=root/name
    includes += [parent]+sorted(p for p in parent.rglob('*') if p.is_dir())
files=['apps/sentry/autonomy/autonomy.c','apps/sentry/app/assembly.c','apps/sentry/cmd/robot_cmd.c',
       'apps/sentry/chassis/chassis.c','apps/sentry/gimbal/gimbal.c','components/motor/dm/dm_motor.c',
       'ports/stm32f4/usb_port.c','ports/freertos/kernel_freertos.c','Src/usbd_cdc_if.c']
for name in files:
    result=subprocess.run(['gcc','-std=c11','-fsyntax-only','-DUSE_HAL_DRIVER','-DSTM32F407xx','-DARM_MATH_CM4',
        '-Wno-pointer-to-int-cast','-Wno-int-to-pointer-cast','-Wno-overflow']+['-I'+str(p) for p in includes]+[str(root/name)],
        text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    if result.returncode:
        print(result.stderr)
        raise SystemExit(result.returncode)
    print(name+': syntax OK', flush=True)
