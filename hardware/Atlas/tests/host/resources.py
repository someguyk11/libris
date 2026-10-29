"""Record an already-built image; never builds, uploads, or opens hardware."""
import argparse
import csv
import hashlib
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('milestone')
p.add_argument('--ram', type=int, required=True)
p.add_argument('--flash', type=int, required=True)
p.add_argument('--release-check', action='store_true', help='Fail if the complete image exceeds 1,000,000 bytes')
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
image = root / '.pio/build/esp32dev/firmware.bin'
log = root / 'ccps_size_log.csv'
rows = list(csv.DictReader(log.open())) if log.exists() else []
size = image.stat().st_size
baseline = int(rows[0]['image_bytes']) if rows else size
previous = int(rows[-1]['image_bytes']) if rows else size
fields = ['milestone', 'image_bytes', 'delta_previous', 'delta_baseline', 'application_bytes', 'static_ram_bytes', 'sha256']
with log.open('a', newline='') as f:
    w = csv.DictWriter(f, fieldnames=fields)
    if not rows:
        w.writeheader()
    w.writerow(dict(zip(fields, [a.milestone, size, size-previous, size-baseline, a.flash, a.ram, hashlib.sha256(image.read_bytes()).hexdigest()])))
print(f'{a.milestone}: image={size} delta={size-previous:+d} baseline={size-baseline:+d} RAM={a.ram} app={a.flash}')
if a.release_check and size > 1_000_000:
    raise SystemExit(f'RELEASE BLOCKED: {size} > 1000000 bytes (excess {size-1_000_000})')
