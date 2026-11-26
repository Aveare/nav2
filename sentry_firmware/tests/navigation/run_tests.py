"""Host tests only; no flashing and no physical serial access."""
import pathlib
import subprocess
import sys
import tempfile

root = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root.parent/'cod_ws/src/cod_nuc_bringup/scripts'))
from nav_wire import Frame, COMMAND

with tempfile.TemporaryDirectory(prefix='sentry_nav_') as build:
    for flags in (['-O0'], ['-Ofast', '-flto']):
        output = pathlib.Path(build)/'navigation_test'
        subprocess.run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', *flags,
                        '-I'+str(root/'tests/navigation/stubs'),
                        '-I'+str(root/'components/navigation'), '-I'+str(root/'apps/sentry/autonomy'),
                        str(root/'components/navigation/nav_protocol.c'),
                        str(root/'apps/sentry/autonomy/autonomy.c'),
                        str(root/'tests/navigation/navigation_test.c'), '-lm', '-o', str(output)], check=True)
        result = subprocess.check_output([str(output)], text=True).splitlines()
        expected = Frame(COMMAND, 7, 123456789, (.25, -.125, .5), 1).encode().hex()
        assert result[0] == expected, (result[0], expected)
        print(' '.join(flags)+': '+result[1]+'; C/Python bytes match')
