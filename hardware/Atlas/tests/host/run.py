"""Compile real Layer 2 sources on the host; never connect to an ESP32.

Usage: python tests/host/run.py --cxx /path/to/clang++
Also accepts g++ or Zig's executable. No packages are installed.
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default=os.environ.get('CXX', 'c++'))
    parser.add_argument('--source-dir', type=Path, help='Optional baseline source snapshot')
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    source = args.source_dir or here.parents[1] / 'src'
    modules = ('Navigation', 'Routes', 'CheckpointManager', 'ObstacleAvoidance')
    temp = Path(os.environ['LOCALAPPDATA']) / 'Temp' / 'opencode' if os.name == 'nt' else Path(tempfile.gettempdir())
    temp.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='atlas-host-', dir=temp) as folder:
        work = Path(folder)
        for name in modules:
            for suffix in ('.h', '.cpp'):
                shutil.copyfile(source / (name + suffix), work / (name + suffix))
        for name in ('MotionController.h', 'SensorManager.h', 'SafetyController.h'):
            shutil.copyfile(source / name, work / name)
        config = (source / 'AtlasConfig.h').read_text(encoding='utf-8')
        # Never include private provisioning. All production software limits stay
        # unchanged; only physical calibration is simulated in this TEMP copy.
        config, count = re.subn(r'#if __has_include\("AtlasSecrets.local.h"\).*?#endif', '', config, count=1, flags=re.S)
        assert count == 1, 'Provisioning include changed; review host isolation'
        for predicate in ('distanceCalibrated', 'turnsCalibrated'):
            config, count = re.subn(r'inline bool ' + predicate + r'\(\) \{[^}]*\}',
                                    'inline bool ' + predicate + '() { return true; }', config, count=1)
            assert count == 1, 'Calibration predicate changed; review host isolation'
        (work / 'AtlasConfig.h').write_text(config, encoding='utf-8')
        executable = work / ('navigation-tests.exe' if os.name == 'nt' else 'navigation-tests')
        command = [args.cxx]
        if Path(args.cxx).stem.lower() == 'zig':
            command.append('c++')
        command += ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-DCONFIG_IDF_TARGET_ESP32=1',
                    '-I' + str(here), '-I' + str(work), str(here / 'navigation_tests.cpp')]
        command += [str(work / (name + '.cpp')) for name in modules]
        command += ['-o', str(executable)]
        subprocess.run(command, check=True)
        return subprocess.run([str(executable)], check=False).returncode


if __name__ == '__main__':
    raise SystemExit(main())
