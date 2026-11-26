"""Host C syntax check of changed production files; not an ARM compile/link."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
includes = [root/'config', root/'Inc']
for name in ['Drivers', 'Middlewares', 'bsp', 'modules', 'application']:
    parent = root/name
    includes += [parent] + sorted(p for p in parent.rglob('*') if p.is_dir())
files = [
    'modules/navigation/nav_protocol.c',
    'modules/navigation/rmcv_pose.c',
    'modules/navigation/ax650_usb.c',
    'modules/navigation/nav_rtos.c',
    'application/autonomy/autonomy.c',
    'application/robot.c',
    'application/cmd/robot_cmd.c',
    'application/chassis/chassis.c',
    'application/gimbal/gimbal.c',
    'modules/motor/DMmotor/dmmotor.c',
    'modules/imu/ins_task.c',
    'bsp/usb/bsp_usb.c',
    'Src/usbd_cdc_if.c',
]
for name in files:
    result = subprocess.run(
        ['gcc', '-std=c11', '-fsyntax-only', '-DUSE_HAL_DRIVER', '-DSTM32F407xx', '-DARM_MATH_CM4',
         '-Wno-pointer-to-int-cast', '-Wno-int-to-pointer-cast', '-Wno-overflow',
         '-Wno-implicit-function-declaration'] + ['-I'+str(p) for p in includes] + [str(root/name)],
        text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        print(result.stderr)
        raise SystemExit(result.returncode)
    print(name + ': syntax OK', flush=True)
