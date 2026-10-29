"""AUDIT ONLY. Build independent diagnostics in a temporary, synthetic source copy.

Never changes production firmware or original tests; never opens a device/network.
The same declared synthetic fixture as tests/host/run.py is used, not calibration.
"""
import argparse
import ast
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--cxx', required=True)
p.add_argument('--optimization', choices=('O0', 'Os'), required=True)
a = p.parse_args()
atlas = Path(__file__).resolve().parents[2]
here = Path(__file__).resolve().parent
host = atlas / 'tests/host'
output = atlas / 'docs/audit-output/2026-10-27'
pure = ('SensorConfidence','ImuIntegration','MotionConfidence','MovementHistory','PoseEstimator','ConfidenceSupervisor')
tree = ast.parse((host/'run.py').read_text())
replacement_node = next(n for n in ast.walk(tree) if isinstance(n,ast.Assign)
                        and any(isinstance(t,ast.Name) and t.id=='replacements' for t in n.targets))
replacements = ast.literal_eval(replacement_node.value)
with tempfile.TemporaryDirectory(prefix='atlas-audit-',dir=Path(os.environ['LOCALAPPDATA'])/'Temp/opencode') as folder:
    work = Path(folder)
    for source in (atlas/'src').iterdir():
        if source.suffix in ('.h','.cpp','.ino') and 'Secrets' not in source.name:
            shutil.copyfile(source,work/source.name)
    config=(work/'AtlasConfig.h').read_text()
    config,count=re.subn(r'#if __has_include\("AtlasSecrets.local.h"\).*?#endif','',config,count=1,flags=re.S)
    assert count==1
    for predicate in ('distanceCalibrated','turnsCalibrated'):
        config,count=re.subn(r'inline bool '+predicate+r'\(\) \{[^}]*\}',
                            'inline bool '+predicate+'() { return true; }',config,count=1)
        assert count==1
    start=config.index('inline const char* pinConfigurationError()')
    end=config.index('inline const char* tuningError()',start)
    config=config[:start]+'inline const char* pinConfigurationError() { return nullptr; }\n'+config[end:]
    for old,new in replacements.items():
        assert old in config, old
        config=config.replace(old,new,1)
    (work/'AtlasConfig.h').write_text(config)
    # Import only original synthetic evidence helpers, not a renamed main (whose
    # implicit return is legal only when the function is actually named main).
    helpers=(host/'confidence_supervisor_tests.cpp').read_text().split('int main() {',1)[0]
    (work/'supervisor_fixture.inc').write_text(helpers)
    # Existing coordinator fixture, with its original main renamed ONLY in TEMP.
    telemetry=(host/'telemetry_integration_tests.cpp').read_text()
    (work/'telemetry_fixture.inc').write_text(telemetry.replace('int main() {','int audit_original_telemetry_main() {',1))
    shutil.copyfile(host/'network_stubs/NetworkManager.h',work/'NetworkManager.h')
    summary=[]
    for name in ('pure_diagnostics','failure_matrix','integration_diagnostics','telemetry_diagnostics'):
        test=here/(name+'.cpp')
        executable=work/(name+'.exe')
        command=[a.cxx]+(['c++'] if Path(a.cxx).stem.lower()=='zig' else [])
        command+=['-std=c++17','-Wall','-Wextra','-Werror','-'+a.optimization,
                  '-DCONFIG_IDF_TARGET_ESP32=1','-DATLAS_HOST_TEST=1',
                  '-I'+str(work),'-I'+str(host/'network_stubs'),'-I'+str(host),str(test)]
        if name in ('integration_diagnostics','telemetry_diagnostics'):
            command += [str(work/(module+'.cpp')) for module in
                        ('Navigation','Routes','CheckpointManager','ObstacleAvoidance','MotionController','SafetyController','ConfidenceIntegration')]
            if name=='telemetry_diagnostics': command += [str(work/'TelemetryManager.cpp')]
        command += [str(atlas/'src'/(module+'.cpp')) for module in pure]+['-o',str(executable)]
        compiled=subprocess.run(command,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        log='COMMAND: '+subprocess.list2cmdline(command)+'\n'+compiled.stdout
        if compiled.returncode:
            log+='COMPILATION FAILED\n'
            result={'suite':name,'compile_exit':compiled.returncode}
        else:
            run=subprocess.run([str(executable)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
            log+='RUN: '+str(executable)+'\n'+run.stdout+f'EXIT: {run.returncode}\n'
            result={'suite':name,'compile_exit':0,'test_exit':run.returncode,'output':run.stdout}
        print(name,a.optimization,'compile_exit='+str(result['compile_exit']))
        print(result.get('output',compiled.stdout))
        (output/('final-'+name+'-'+a.optimization+'.log')).write_text(log)
        summary.append(result)
    (output/('final-diagnostics-'+a.optimization+'.json')).write_text(json.dumps(summary,indent=2)+'\n')
    raise SystemExit(1 if any(r.get('compile_exit') or r.get('test_exit') for r in summary) else 0)
