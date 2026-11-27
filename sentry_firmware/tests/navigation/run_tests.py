"""Host tests only; no flashing and no physical serial access."""
import pathlib
import subprocess
import sys
import tempfile

root = pathlib.Path(__file__).resolve().parents[2]
for candidate in (root.parent/'src/cod_nuc_bringup/scripts',
                  root.parent/'cod_ws/src/cod_nuc_bringup/scripts'):
    if candidate.is_dir():
        sys.path.insert(0, str(candidate))
        break
from nav_wire import Frame, COMMAND
from rmcv_wire import FullPoseFrame, TargetFrame

with tempfile.TemporaryDirectory(prefix='sentry_nav_') as build:
    for feedback_only in (True, False):
        for flags in (['-O0'], ['-Ofast', '-flto']):
            mode = 'feedback-only' if feedback_only else 'drive-enabled'
            output = pathlib.Path(build)/('navigation_test_'+mode)
            defines = ['-DSENTRY_AUTONOMY_TEST_FEEDBACK_ONLY=1'] if feedback_only else []
            subprocess.run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', *flags,
                            *defines,
                            '-I'+str(root/'tests/navigation/stubs'),
                            '-I'+str(root/'components/navigation'), '-I'+str(root/'apps/sentry/autonomy'),
                            '-I'+str(root/'components/ax650'),
                            str(root/'components/navigation/nav_protocol.c'),
                            str(root/'components/ax650/ax650_pose.c'),
                            str(root/'components/ax650/ax650_usb.c'),
                            str(root/'apps/sentry/autonomy/autonomy.c'),
                            str(root/'tests/navigation/navigation_test.c'), '-lm', '-o', str(output)], check=True)
            result = subprocess.check_output([str(output)], text=True).splitlines()
            expected_nav = Frame(COMMAND, 7, 123456789, (.25, -.125, .5), 1).encode().hex()
            expected_target = TargetFrame(3, 30.0, -5.0, .25, -.125, 1.0, -2.0,
                                          4.5, 1, 3).encode().hex()
            expected_pose = FullPoseFrame(
                0, 1.0, 0.0, 0.0, 0.0,
                12.0 * 0.01745329252, -3.0 * 0.01745329252, 8.0 * 0.01745329252,
                0.0, 950, 949, 948, 947, 0x021f, 0, 0).encode().hex()
            assert result[0] == expected_nav, (result[0], expected_nav)
            assert result[1] == expected_target, (result[1], expected_target)
            assert result[2] == expected_pose, (result[2], expected_pose)
            print(mode+' '+' '.join(flags)+': '+result[3]+'; NV/0x0503/0x1022 C/Python bytes match')
