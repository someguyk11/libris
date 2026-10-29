"""Generate isolated size probes below the approved OpenCode temporary directory.

baseline: committed pre-CCPS source with -Os, otherwise unchanged.
features: synthetic configuration keeps code paths linkable, but setup/loop are
INERT. Original entry points are explicit linker roots and can NEVER be called
by the probe entry points. No private provisioning is copied. Never upload probes.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('profile', choices=('baseline', 'features'))
parser.add_argument('--label', default='', help='Optional new experiment suffix; existing directories are never overwritten')
parser.add_argument('--revision', default='HEAD', help='Git revision for the baseline profile; unused for features')
args = parser.parse_args()
atlas = Path(__file__).resolve().parents[2]
repo = atlas.parents[1]
temp = Path(os.environ['LOCALAPPDATA']) / 'Temp/opencode'
assert re.fullmatch(r'[A-Za-z0-9_-]*',args.label)
dest = temp / ('atlas-ccps-probe-' + args.profile + ('-'+args.label if args.label else ''))
dest.mkdir(parents=True, exist_ok=False)  # never overwrite an existing experiment
(dest / 'src').mkdir()
if args.profile == 'baseline':
    names = subprocess.check_output(['git','ls-tree','-r','--name-only',args.revision,'hardware/Atlas/src'],cwd=repo,text=True).splitlines()
    for name in names:
        if Path(name).name == 'AtlasSecrets.local.h':
            continue
        content = subprocess.check_output(['git','show',args.revision+':'+name],cwd=repo)
        (dest / 'src' / Path(name).name).write_bytes(content)
else:
    for source in (atlas / 'src').iterdir():
        if source.suffix in ('.h','.cpp','.ino') and 'Secrets' not in source.name:
            (dest / 'src' / source.name).write_bytes(source.read_bytes())

config_file = dest / 'src/AtlasConfig.h'
config = config_file.read_text()
config = re.sub(r'#if __has_include\("AtlasSecrets.local.h"\).*?#endif','',config,count=1,flags=re.S)
if args.profile == 'features':
    replacements = {
        'constexpr MotorPins LeftMotor = {UnconfiguredPin, UnconfiguredPin, false};':'constexpr MotorPins LeftMotor = {18,19,false};',
        'constexpr MotorPins RightMotor = {UnconfiguredPin, UnconfiguredPin, false};':'constexpr MotorPins RightMotor = {21,22,false};',
        'constexpr bool MotorInputFanoutConfirmed = false;':'constexpr bool MotorInputFanoutConfirmed = true;',
        'constexpr int DriverSleepPin = UnconfiguredPin;':'constexpr int DriverSleepPin = 27;',
        'constexpr int LeftEncoderPin = UnconfiguredPin;':'constexpr int LeftEncoderPin = 32;',
        'constexpr int RightEncoderPin = UnconfiguredPin;':'constexpr int RightEncoderPin = 33;',
        'constexpr EncoderEdges CountEdges = EncoderEdges::Unconfigured;':'constexpr EncoderEdges CountEdges = EncoderEdges::Rising;',
        'constexpr bool SingleSignalEncodersConfirmed = false;':'constexpr bool SingleSignalEncodersConfirmed = true;',
        'constexpr bool SensorElectricalInterfaceConfirmed = false;':'constexpr bool SensorElectricalInterfaceConfirmed = true;',
        'constexpr int ImuSdaPin = UnconfiguredPin;':'constexpr int ImuSdaPin = 4;',
        'constexpr int ImuSclPin = UnconfiguredPin;':'constexpr int ImuSclPin = 23;',
        'constexpr uint8_t ImuAddress = 0;':'constexpr uint8_t ImuAddress = 0x6A;',
        'constexpr bool ImuElectricalInterfaceConfirmed = false;':'constexpr bool ImuElectricalInterfaceConfirmed = true;',
        'constexpr float LeftCountsPerWheelRevolution = 0.0f;':'constexpr float LeftCountsPerWheelRevolution = 100.0f;',
        'constexpr float RightCountsPerWheelRevolution = 0.0f;':'constexpr float RightCountsPerWheelRevolution = 100.0f;',
        'constexpr float WheelTrackCm = 0.0f;':'constexpr float WheelTrackCm = 10.0f;',
        'constexpr float ClockwiseTurnScale = 0.0f;':'constexpr float ClockwiseTurnScale = 1.0f;',
        'constexpr float CounterclockwiseTurnScale = 0.0f;':'constexpr float CounterclockwiseTurnScale = 1.0f;',
        'constexpr float BackendStepCm = 0.0f;':'constexpr float BackendStepCm = 10.0f;',
        'constexpr bool HardwareTestMode = false;':'constexpr bool HardwareTestMode = true;',
        '#define ATLAS_DEVICE_KEY ""':'#define ATLAS_DEVICE_KEY "00000000000000000000000000000000"',
        '#define ATLAS_TLS_CA ""':'#define ATLAS_TLS_CA "-----BEGIN CERTIFICATE-----\\nINVALID_LINK_PROBE_ONLY\\n-----END CERTIFICATE-----"',
        'constexpr AtlasConfidence::ImuParameters ImuNavigationParameters{};':'''constexpr AtlasConfidence::ImuParameters ImuNavigationParameters = [] {
            AtlasConfidence::ImuParameters p; p.mounting[0]=p.mounting[4]=p.mounting[8]=1;
            p.stationaryRate=0.1f; p.accelerationToleranceMg=20; p.maximumNoise=0.01f;
            p.calibrationSamples=4; p.maximumGapUs=40000; p.calibrationValidityUs=100000000; return p; }();''',
        'constexpr AtlasConfidence::SupervisorParameters ConfidenceCalibration{};':'''constexpr AtlasConfidence::SupervisorParameters ConfidenceCalibration = [] {
            AtlasConfidence::SupervisorParameters p; p.motion={2,0.2f,0.2f,0.3f,0.5f};
            p.encoder.maximumEdgesPerSecond=10000; p.encoder.progressMismatch=0.2f;
            p.pose={0.01f,0.001f,0.01f,0.001f,0.001f,0.001f,0.001f,40000};
            p.maximumPositionVariance=100; p.maximumYawVariance=10; p.warningConfidence=0.5f;
            p.historyBias=p.historySlope=1; p.routeDistanceTolerance=10; p.routeYawTolerance=0.5f;
            p.alignmentUs=40000; p.cautionDuty=50; p.warningSamples=3; p.verificationSamples=2; p.recoveryLimit=2; return p; }();''',
    }
    for old,new in replacements.items():
        assert old in config, 'Probe anchor changed: '+old
        config = config.replace(old,new,1)
    config,count = re.subn(r'constexpr SensorPins Sensors\[SensorCount\] = \{.*?\};',
        'constexpr SensorPins Sensors[SensorCount] = {{13,34},{14,35},{25,36},{26,39}};',config,count=1,flags=re.S)
    assert count == 1
    ino_file = dest / 'src/Atlas.ino'
    ino = ino_file.read_text().replace('void setup() {','void atlas_probe_original_setup() {',1).replace('void loop() {','void atlas_probe_original_loop() {',1)
    ino += '''
// INERT SIZE PROBE. The original firmware is retained ONLY as linker roots.
extern "C" { void (*volatile atlas_resource_roots[])() __attribute__((used)) = {atlas_probe_original_setup,atlas_probe_original_loop}; }
void setup() { for(;;) delay(1000); }
void loop() { delay(1000); }
'''
    ino_file.write_text(ino)
config_file.write_text(config)
(dest / 'partitions_atlas_v1.csv').write_bytes((atlas / 'partitions_atlas_v1.csv').read_bytes())
root_flag = '    -Wl,-u,atlas_resource_roots\n' if args.profile == 'features' else ''
ini = f'''[platformio]
libdeps_dir = {(atlas / '.pio/libdeps').as_posix()}
[env:esp32dev]
platform = espressif32@8.0.0
board = esp32dev
framework = arduino
board_build.partitions = partitions_atlas_v1.csv
build_flags =
    -DARDUINO_USB_MODE=0
    -DARDUINO_USB_CDC_ON_BOOT=0
    -Os
{root_flag}lib_deps = bblanchon/ArduinoJson@^7.0.0
'''
(dest / 'platformio.ini').write_text(ini)
print(dest)
print('NOT FOR DEPLOYMENT OR HARDWARE TESTING')
