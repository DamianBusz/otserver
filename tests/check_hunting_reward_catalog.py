#!/usr/bin/env python3
"""Validate production bag pools against authoritative cosmetic identities.
Run from the server root: python3 tests/check_hunting_reward_catalog.py /path/to/luajit
"""
import subprocess
import sys
import xml.etree.ElementTree as ET

script = '''
GameStore=dofile('data/libs/gamestore/constants.lua')
GameStore.Categories={dofile('data/modules/scripts/gamestore/catalog/cosmetics_outfits.lua'),dofile('data/modules/scripts/gamestore/catalog/cosmetics_mounts.lua'),dofile('data/modules/scripts/gamestore/catalog/tournament_exclusive_offers.lua')}
function Action() return {id=function()end,register=function()end} end
package.loaded['gamestore.constants']=GameStore
GameStore={} -- Deliberately stale legacy global, as in production startup.
dofile('data/scripts/actions/items/hunting_shop_rewards.lua')
local p={getOwnedOutfitAddons=function()return nil end,ownsMount=function()return false end}
for _,o in ipairs(HuntingShopRewards.outfitPool(p))do print('outfit',o.male,o.female,o.name)end
for _,m in ipairs(HuntingShopRewards.mountPool(p))do print('mount',m.id,m.name)end
'''
result = subprocess.run([sys.argv[1] if len(sys.argv)>1 else 'luajit', '-'], input=script, text=True, capture_output=True, check=True)
outfits = {int(x.attrib['looktype']):x.attrib for x in ET.parse('data/XML/outfits.xml').getroot()}
mounts = {int(x.attrib['id']):x.attrib for x in ET.parse('data/XML/mounts.xml').getroot()}
counts = {'outfit':0, 'mount':0}
seen = set()
for line in result.stdout.splitlines():
    fields=line.split('\t');kind=fields[0];counts[kind]+=1
    identity=tuple(fields[:3] if kind=='outfit' else fields[:2])
    assert identity not in seen, ('duplicate',identity)
    seen.add(identity)
    if kind=='mount':
        mount=mounts[int(fields[1])]
        assert mount.get('type')=='store', (fields,mount)
        assert mount['name'].casefold()==fields[2].casefold(), (fields,mount)
    else:
        for look in map(int,fields[1:3]):
            assert outfits[look].get('enabled')=='yes' and outfits[look].get('from')=='store', (fields,outfits[look])
assert counts['outfit'] > 0 and counts['mount'] > 0
print('Verified Store-only cosmetic pools and identities:',counts)
