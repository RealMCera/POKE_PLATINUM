#!/usr/bin/env python3
from pathlib import Path
import json

root = Path('.').resolve()
slots = (root / 'generated/trainers.txt').read_text().splitlines()[:855]
changed = 0
for slot in slots:
    p = root / 'res/trainers/data' / (slot.removeprefix('TRAINER_').lower() + '.json')
    d = json.loads(p.read_text())
    party = d.get('party', [])
    if any(mon.get('item') not in (None, 'ITEM_NONE') for mon in party):
        dirty = False
        for mon in party:
            if mon.get('item') is None:
                mon['item'] = 'ITEM_NONE'
                dirty = True
        if dirty:
            p.write_text(json.dumps(d, indent=4) + '\n')
            changed += 1
print(f'[Rare Emerald] fixed item placeholders in {changed} trainer files')
