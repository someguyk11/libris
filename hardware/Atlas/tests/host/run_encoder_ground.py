"""Real encoder-only control, safety, navigation, local commands and route storage.

Only GPIO/PWM, time, queues and Preferences backing are simulated. No hardware I/O.
Usage: python tests/host/run_encoder_ground.py --cxx /path/to/zig.exe
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
    parser.add_argument('--cxx', required=True)
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    root = here.parents[1]
    json_include = root / '.pio/libdeps/atlas-local-drive/ArduinoJson/src'
    assert json_include.is_dir(), 'Build atlas-local-drive first to resolve ArduinoJson'
    temp = Path(os.environ['LOCALAPPDATA']) / 'Temp/opencode'
    modules = ('MotionController', 'SensorManager', 'SafetyController', 'Navigation',
               'Routes', 'CheckpointManager', 'ObstacleAvoidance', 'LocalDrive', 'LocalRouteStore')
    with tempfile.TemporaryDirectory(prefix='atlas-ground-', dir=temp) as folder:
        work = Path(folder)
        for name in modules:
            for suffix in ('.h', '.cpp'):
                shutil.copyfile(root / 'src' / (name + suffix), work / (name + suffix))
        for name in ('LocalRoutePolicy.h', 'LocalRouteJson.h'):
            shutil.copyfile(root / 'src' / name, work / name)
        config = (root / 'src/AtlasConfig.h').read_text(encoding='utf-8')
        config, count = re.subn(r'#if __has_include\("AtlasSecrets.local.h"\).*?#endif', '', config, count=1, flags=re.S)
        assert count == 1  # Never load private provisioning in host tests.
        (work / 'AtlasConfig.h').write_text(config, encoding='utf-8')
        (work / 'Arduino.h').write_text(r'''#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#define ARDUINO_ISR_ATTR
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define CHANGE 3
#define RISING 4
#define FALLING 5
uint32_t millis();
void pinMode(int,int);
void digitalWrite(int,int);
int digitalRead(int);
void delayMicroseconds(uint32_t);
void attachInterruptArg(int,void(*)(void*),void*,int);
bool ledcAttach(int,uint32_t,uint8_t);
bool ledcWrite(int,uint32_t);
bool ledcDetach(int);
inline size_t strlcpy(char* dst,const char* src,size_t n) {
    const size_t len=std::strlen(src);
    if(n) { const size_t copy=len<n-1?len:n-1; std::memcpy(dst,src,copy); dst[copy]=0; }
    return len;
}
''', encoding='utf-8')
        (work / 'freertos').mkdir()
        (work / 'freertos/FreeRTOS.h').write_text('''#pragma once
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
#define portENTER_CRITICAL_ISR(x) ((void)(x))
#define portEXIT_CRITICAL_ISR(x) ((void)(x))
#define pdTRUE 1
#define pdPASS 1
''', encoding='utf-8')
        (work / 'freertos/portmacro.h').write_text('#include "FreeRTOS.h"\n', encoding='utf-8')
        (work / 'freertos/queue.h').write_text(r'''#pragma once
#include "FreeRTOS.h"
#include <vector>
#include <cstring>
struct FakeQueue { std::vector<unsigned char> bytes; bool full=false; };
using QueueHandle_t=FakeQueue*;
inline QueueHandle_t xQueueCreate(unsigned n,size_t size) { return n==1?new FakeQueue{std::vector<unsigned char>(size),false}:nullptr; }
inline int xQueueOverwrite(QueueHandle_t q,const void* p) { std::memcpy(q->bytes.data(),p,q->bytes.size()); q->full=true; return pdPASS; }
inline int xQueueSend(QueueHandle_t q,const void* p,unsigned) { return q->full?0:xQueueOverwrite(q,p); }
inline int xQueuePeek(QueueHandle_t q,void* p,unsigned) { if(!q->full)return 0; std::memcpy(p,q->bytes.data(),q->bytes.size()); return pdTRUE; }
inline int xQueueReceive(QueueHandle_t q,void* p,unsigned t) { if(!xQueuePeek(q,p,t))return 0; q->full=false; return pdTRUE; }
''', encoding='utf-8')
        (work / 'Preferences.h').write_text(r'''#pragma once
#include <map>
#include <string>
#include <cstring>
class Preferences {
    std::string ns;
    inline static std::map<std::string,std::string> data;
public:
    bool begin(const char* name,bool) { ns=name; return true; }
    size_t getStringLength(const char* key) { const auto it=data.find(ns+key); return it==data.end()?0:it->second.size()+1; }
    size_t getString(const char* key,char* out,size_t cap) { const auto it=data.find(ns+key); if(it==data.end()||it->second.size()+1>cap)return 0; std::memcpy(out,it->second.c_str(),it->second.size()+1); return it->second.size()+1; }
    size_t putString(const char* key,const char* value) { data[ns+key]=value; return std::strlen(value); }
};
''', encoding='utf-8')
        (work / 'esp_random.h').write_text('''#pragma once
#include <cstddef>
inline void esp_fill_random(void* out,size_t size) { auto* p=static_cast<unsigned char*>(out); for(size_t i=0;i<size;++i)p[i]=static_cast<unsigned char>(i+1); }
''', encoding='utf-8')
        command = [args.cxx]
        if Path(args.cxx).stem.lower() == 'zig':
            command.append('c++')
        command += ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-DCONFIG_IDF_TARGET_ESP32=1',
                    '-DATLAS_ENCODER_GROUND_TEST=1', '-I' + str(work), '-I' + str(here),
                    '-I' + str(json_include), str(here / 'encoder_ground_tests.cpp')]
        command += [str(work / (name + '.cpp')) for name in modules]
        executable = work / 'encoder-ground-tests.exe'
        command += ['-o', str(executable)]
        subprocess.run(command, check=True)
        return subprocess.run([str(executable)], check=False).returncode


if __name__ == '__main__':
    raise SystemExit(main())
