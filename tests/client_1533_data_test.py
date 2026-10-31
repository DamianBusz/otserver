"""Cross-check copied assets, XML definitions and executable Lua registrations.

Usage: python3 tests/client_1533_data_test.py /path/to/luajit
"""
from pathlib import Path
import hashlib
import json
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
manifest = json.loads((ROOT / 'data/items/client-15.33-manifest.json').read_text())
creatures = json.loads((ROOT / 'data/items/client-15.33-creatures.json').read_text())['creatures']
for filename, source in manifest['assets'].items():
    data = (ROOT / filename).read_bytes()
    assert len(data) == source['bytes'], filename
    assert hashlib.sha256(data).hexdigest() == source['sha256'], filename

items = {}
for item in ET.parse(ROOT / 'data/items/items.xml').getroot():
    if 'id' in item.attrib:
        items.setdefault(int(item.attrib['id']), []).append(item)

def attributes(item):
    return {a.attrib['key'].lower(): a.attrib.get('value') for a in item.findall('attribute')}

for gear in manifest['moonsilver']:
    definitions = items[gear['id']]
    assert len(definitions) == 1, gear
    item = definitions[0]
    assert item.attrib['name'] == gear['name']
    attrs = attributes(item)
    assert attrs.get('imbuementslot') in ('2', '3'), gear
    assert 'moveevent' in attrs['script'], gear
    script = next(a for a in item if a.attrib.get('key') == 'script')
    restrictions = attributes(script)
    assert restrictions['level'] == ('1000' if gear['proficiency'] else '800'), gear
    assert restrictions['vocation'], gear

# Actual Lua is executed: misspelled engine constants, bad syntax, duplicate
# registrations and broken loot/corpse references fail before deployment.
registered_constants = set()
for path in (ROOT / 'src').rglob('*.hpp'):
    registered_constants.update(re.findall(r'\b(?:COMBAT_|CONST_ME_|BESTY_RACE_)[A-Z_]+\b', path.read_text()))
for path in (ROOT / 'src/lua').rglob('*.cpp'):
    registered_constants.update(re.findall(r'\b(?:COMBAT_|CONST_ME_|BESTY_RACE_)[A-Z_]+\b', path.read_text()))
source = ['local definitions = {}', 'Game = { createMonsterType = function(name)',
          'assert(not definitions[name], "duplicate " .. name)',
          'return { register = function(_, monster) definitions[name] = monster end } end }']
source += [f'{name} = {json.dumps(name)}' for name in sorted(registered_constants)]
source += ['setmetatable(_G, {__index = function(_, key) error("Unknown global " .. key) end})']
for creature in creatures:
    identity = next(c for c in manifest['creatures'] if c['id'] == creature['race_id'])
    source += [f'dofile({json.dumps(creature["script"])})',
               f'local m = assert(definitions[{json.dumps(creature["name"])}])',
               f'assert(m.outfit.lookType == {identity["looktype"]})',
               f'assert(m.outfit.lookAddons == {identity["lookAddons"]})',
               f'assert(m.health == {creature["health"]} and m.maxHealth == m.health)',
               f'assert(m.experience == {creature["experience"]})',
               'assert(m.health > 0 and #m.attacks > 0)',
               'for _, a in ipairs(m.attacks) do assert(a.interval > 0 and a.chance > 0 and a.chance <= 100) end',
               'for _, a in ipairs(m.loot) do assert(a.chance >= 0 and a.chance <= 100000) end']
    if not identity['boss']:
        source += [f'assert(m.raceId == {identity["id"]} and m.Bestiary.Stars == {identity["difficulty"]})']
    for n, part in enumerate(('Head', 'Body', 'Legs', 'Feet'), 1):
        source += [f'assert(m.outfit.look{part} == {identity["colors"][str(n)][0]})']
    corpse = items[creature['corpse']]
    assert len(corpse) == 1, creature
    assert int(attributes(corpse[0])['containersize']) >= 30, creature
    script = (ROOT / creature['script']).read_text()
    for item_id in re.findall(r'\{ id = (\d+), chance', script):
        assert int(item_id) in items, (creature, item_id)
assert len(creatures) == 27
subprocess.run([sys.argv[1], '-'], input='\n'.join(source), text=True, cwd=ROOT, check=True)
print('15.33: asset hashes, 27 equipment definitions, 27 Lua creature registrations and loot references passed')
