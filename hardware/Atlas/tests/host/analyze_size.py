"""Read actual artifacts and attribute allocated GNU-map inputs (not symbol aliases).

No build/upload/hardware access. Debug/noload sections and linker padding are not
attributed to archives. The complete binary remains the release-size authority.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build-dir', type=Path, default=Path('.pio/build/esp32dev'))
p.add_argument('--output', type=Path)
p.add_argument('--summary', action='store_true')
a = p.parse_args()
image = a.build_dir / 'firmware.bin'
mapping = a.build_dir / 'firmware.map'
allocated = {'.flash.text', '.flash.rodata', '.flash.appdesc', '.eh_frame',
             '.iram0.text', '.iram0.vectors', '.dram0.data', '.rtc.force_slow'}
archives, objects, sections = Counter(), Counter(), Counter()
current, input_section, in_map = '', '', False
covered = {}
for line in mapping.read_text(errors='replace').splitlines():
    if line == 'Linker script and memory map':
        in_map = True
    if not in_map:
        continue
    if line.startswith('.'):
        current = line.split()[0]
    if current not in allocated:
        continue
    if re.match(r'^ \.\S+', line):
        input_section = line.split()[0]
    match = re.match(r'^\s+(?:\.\S+\s+)?0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(.+?(?:\.o|\)))\s*$', line)
    if not match:
        continue
    address, size, obj = int(match[1], 16), int(match[2], 16), match[3].replace('\\', '/')
    # GNU ld pools strings and may list overlapping relaxed inputs. Count bytes
    # once, and do not falsely charge the complete shared pool to GPIO's object.
    intervals = covered.setdefault(current, [])
    end, unique = address + size, size
    for low, high in intervals:
        unique -= max(0, min(end, high) - max(address, low))
    if unique <= 0:
        continue
    intervals.append((address, end))
    intervals.sort()
    merged = []
    for low, high in intervals:
        if merged and low <= merged[-1][1]:
            merged[-1] = (merged[-1][0], max(high, merged[-1][1]))
        else:
            merged.append((low, high))
    covered[current] = merged
    size = unique
    if re.search(r'\.str\d+\.\d+$', input_section):
        archive, obj = '<merged string pool>', '<shared strings; original object ownership unavailable>'
    elif '.a(' in obj:
        archive = obj.split('.a(', 1)[0].rsplit('/', 1)[-1] + '.a'
    elif '/src/' in obj:
        archive = '<Atlas application>'
    else:
        archive = '<standalone objects>'
    archives[archive] += size
    objects[obj] += size
    sections[current] += size
data = dict(image_bytes=image.stat().st_size, target_bytes=1_000_000,
            release_size_pass=image.stat().st_size <= 1_000_000,
            sha256=hashlib.sha256(image.read_bytes()).hexdigest(),
            attributed_sections=dict(sections), allocated_inputs_by_archive=dict(archives.most_common()),
            largest_input_objects=dict(objects.most_common(25)),
            note='Unique allocated input bytes; merged strings separated; other overlapping inputs attributed to first map entry. Excludes debug/noload, padding and generated data. Not partition capacity.')
if a.output:
    a.output.write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')
if a.summary:
    print(json.dumps({key:data[key] for key in ('image_bytes','target_bytes','release_size_pass','sha256')},indent=2))
    print('Largest uniquely attributed allocated inputs:')
    for name,size in list(archives.most_common())[:12]:
        print(f'{size:9d} {name}')
else:
    print(json.dumps(data, indent=2))
