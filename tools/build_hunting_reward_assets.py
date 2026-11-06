#!/usr/bin/env python3
"""Build private item IDs 60001-60006 from known Appearance protobuf fields.

Usage: build_hunting_reward_assets.py UPSTREAM_APPEARANCES [--client-root PATH]
The input must match the upstream hash in client-15.33-manifest.json. The
client's downloaded banks are never rewritten; it loads the small overlay.
"""
import argparse
import hashlib
import json
from pathlib import Path


def varint(value):
    result = bytearray()
    while value >= 128:
        result.append((value & 127) | 128)
        value >>= 7
    result.append(value)
    return bytes(result)


def integer(field, value):
    return varint(field << 3) + varint(value)


def message(field, value):
    return varint(field << 3 | 2) + varint(len(value)) + value


def text(field, value):
    return message(field, value.encode())


def appearance(item_id, name, description, container=False, phase_ms=350):
    frames = 4 if container else 1
    info = b''.join(integer(field, 1) for field in (1, 2, 3, 4))
    info += integer(5, 201033 if container else 222597) * frames
    info += integer(7, 32) + integer(8, 0)
    if frames > 1:
        phase = integer(1, phase_ms) + integer(2, phase_ms)
        info += message(6, integer(2, 1) + integer(4, 0) + message(6, phase) * frames)
    frame = integer(1, 0) + integer(2, 0) + message(3, info)
    flags = integer(7, 1) + integer(18, 1)
    if container:
        flags += integer(5, 1) + message(34, integer(1, 3))
    item = integer(1, item_id) + message(2, frame) + message(3, flags)
    return message(1, item + text(4, name) + text(5, description))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('upstream', type=Path)
    parser.add_argument('--client-root', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    manifest_path = root / 'data/items/client-15.33-manifest.json'
    manifest = json.loads(manifest_path.read_text())
    entry = manifest['assets']['data/items/appearances.dat']
    upstream_hash = entry.get('upstream_sha256', entry['sha256'])
    upstream = args.upstream.read_bytes()
    assert hashlib.sha256(upstream).hexdigest() == upstream_hash, 'Use the original upstream appearance bank'
    overlay = appearance(60001, 'full outfit reward bag', 'Open to unlock a random unowned full store outfit, including both addons.')
    overlay += appearance(60002, 'demonhide backpack', 'A demonic backpack with 24 slots and a pulsing ember glow.', True)
    overlay += appearance(60003, 'abyssal demon backpack', 'An obsidian demon backpack with 28 slots and a pulsing violet glow.', True)
    overlay += appearance(60004, 'premium mount reward bag', 'Open to unlock one random unowned premium Store mount.')
    overlay += appearance(60005, 'demon skull backpack', 'A horned demon-skull backpack with 24 slots and pulsing ember eyes.', True, 300)
    overlay += appearance(60006, 'goblin skull backpack', 'A goblin-skull backpack with 28 slots, stitched leather and pulsing green eyes.', True, 450)
    path = root / 'data/items/hunting-rewards/appearances.dat'
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(overlay)
    merged = upstream + overlay
    (root / 'data/items/appearances.dat').write_bytes(merged)
    entry.update(upstream_sha256=upstream_hash, custom_overlay='data/items/hunting-rewards/appearances.dat',
                 custom_overlay_sha256=hashlib.sha256(overlay).hexdigest(),
                 sha256=hashlib.sha256(merged).hexdigest(), bytes=len(merged))
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
    if args.client_root:
        client = args.client_root / 'modules/game_things/hunting_rewards'
        client.mkdir(parents=True, exist_ok=True)
        (client / 'appearances.dat').write_bytes(overlay)
        (client / 'items.otml').write_text('items\n  60001\n    image: /game_things/hunting_rewards/outfit-reward-bag-v2\n  60002\n    image: /game_things/hunting_rewards/demonhide-backpack-v2\n    image-pulse: true\n  60003\n    image: /game_things/hunting_rewards/abyssal-demon-backpack-v2\n    image-pulse: true\n  60004\n    image: /game_things/hunting_rewards/mount-reward-bag-v1\n  60005\n    image: /game_things/hunting_rewards/demon-skull-backpack-v1\n    image-pulse: true\n  60006\n    image: /game_things/hunting_rewards/goblin-skull-backpack-v1\n    image-pulse: true\n')
    print('Generated private item IDs 60001-60006; upstream client assets are unchanged.')


if __name__ == '__main__':
    main()
