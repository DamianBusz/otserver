"""Run from the server root: python3 tests/knight_healing_data_test.py ../otclient.

Pins existing Knight healing perks to the installed official client's data.
Some perks were already doubled in the 15.25 import; multiplying them again is
incorrect. Explicit spell augments and skill-based additions have distinct units.
"""
import json
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

fixture = json.loads(Path('tests/fixtures/knight-healing-proficiencies.json').read_text())
paths = [Path('data/items/proficiencies.json')]
if len(sys.argv) > 1:
    assets = Path(sys.argv[1]) / 'data/things/1525'
    catalog = json.loads((assets / 'catalog-content.json').read_text())
    paths.extend(assets / entry['file'] for entry in catalog if entry['type'] == 'proficiencies')
    assert len(paths) == 2, 'Expected one catalog-selected client proficiency table'

for path in paths:
    by_id = {entry['ProficiencyId']: entry for entry in json.loads(path.read_text())}
    checked = 0
    for row in fixture['rows']:
        entry = by_id[row['id']]
        assert entry['Name'] == row['name']
        actual = entry['Levels'][row['level']]['Perks'][row['perk']]
        assert actual == row['value'], (path, row['name'], actual, row['value'])
        checked += 1
    assert checked == 38, (path, checked)

    # Cooldown values are seconds, already reduced from 150 to 30 in 15.25.
    for profile in [238, 407]:
        perk = by_id[profile]['Levels'][1]['Perks'][0]
        assert perk == {'Type': 5, 'AugmentType': 6, 'SpellId': 158, 'Value': -30.0}

items = ET.parse('data/items/items.xml')
cooldowns = []
for item in items.getroot().findall('item'):
    for augment in item.findall("attribute[@key='augments']/attribute"):
        if augment.get('key') == 'intense wound cleansing' and augment.get('value') == 'cooldown':
            cooldowns.append((int(item.get('id')), int(augment.find("attribute[@key='value']").get('value'))))
assert cooldowns == [(44620, 24000)], cooldowns
print(f'PASS: {checked} official Knight proficiency perks per table and Intense equipment cooldown')
