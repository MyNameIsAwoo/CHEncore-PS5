#!/usr/bin/env python3
"""Emit note variant layers from export-pc-note-assets.py's source manifest."""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def rotate_scale_translate(v, transform):
    v = [v[i] * transform['m_LocalScale'][axis] for i, axis in enumerate('xyz')]
    q = transform['m_LocalRotation']
    u = [q[a] for a in 'xyz']
    w = q['w']
    dot = sum(u[i] * v[i] for i in range(3))
    norm = sum(x * x for x in u)
    cross = [u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0]]
    return [2*dot*u[i] + (w*w-norm)*v[i] + 2*w*cross[i] +
            transform['m_LocalPosition'][axis] for i, axis in enumerate('xyz')]

def emit(manifest):
    sprites = manifest['sprites']
    roles = manifest['roles']
    roots = manifest['note_layers']
    width, height = manifest['atlas_size']

    def f(v):
        return f'{float(v):.9f}f'

    def vector(values):
        return '{' + ','.join(f(v) for v in values) + '}'

    def layer(role, root_name):
        sprite_id = roles[role]
        sprite = sprites[str(sprite_id)]
        root = roots[root_name]
        x, y, w, h = sprite['atlas_rect']
        vertices = []
        for vertex in sprite['vertices']:
            v = [-vertex[i] if root['flip_x'] and i == 0 else vertex[i] for i in range(3)]
            v = [-v[i] if root['flip_y'] and i == 1 else v[i] for i in range(3)]
            for transform in root['transform_chain']:
                v = rotate_scale_translate(v, transform)
            vertices.append(v)
        rgba = [root['color'][channel] for channel in 'rgba']
        tint = max(rgba[:3]) - min(rgba[:3]) > .1
        enabled = root['enabled']
        moving_head = root_name == 'head'
        uv = [x/width, y/height, (x+w)/width, (y+h)/height]
        pos = ','.join(vector(v) for v in vertices)
        return (f'{{{vector(uv)}, {{{pos}}}, {vector(rgba)}, '
                f'{str(tint).lower()}, {str(enabled).lower()}, '
                f'{str(moving_head).lower()}, false}}')

    declarations = {
        'tapBody': ('tap_body', 'body'),
        'tapCap': ('tap_cap', 'head'),
        'hopoCap': ('hopo_cap', 'head'),
        'starCap': ('star_cap', 'head'),
        'starTapCap': ('star_tap_cap', 'head'),
        'starHopoCap': ('star_hopo_cap', 'head'),
        'openBase': ('open_base', 'head'),
        'openBody': ('open_body', 'body'),
        'openHopo': ('open_hopo', 'head'),
    }
    lines = ['#pragma once', '// Generated from the PC sprite manifest; do not edit.',
             'namespace pc_guitar {']
    for name, (role, root) in declarations.items():
        lines.append(f'inline constexpr Layer {name} = {layer(role, root)};')
    arrays = [
        ('openBodyFrames', 'open_body_frames', 'body'),
        ('starBodyFrames', 'star_body_frames', 'body'),
        ('starOpenBodyFrames', 'star_open_body_frames', 'body'),
        ('starTapBodyFrames', 'star_tap_body_frames', 'body'),
        ('starBottomFrames', 'star_bottom_frames', 'body'),
    ]
    for name, role, root in arrays:
        lines.append(f'inline constexpr Layer {name}[] = {{')
        for _sprite_id in roles[role]:
            # Use the same root geometry; replace just its atlas UV by matching
            # the frame sprite record from the manifest.
            # The frame sprite can have a different pivot, so emit its geometry.
            old = roles[role]
            # Reuse layer() with a temporary role ID while preserving its API.
            roles[role] = _sprite_id
            lines.append(layer(role, root) + ',')
            roles[role] = old
        lines.append('};')
    lines += ['inline constexpr TailRegion openTailRegions[] = {']
    for sprite_id in roles['sustains'][4:6]:
        sprite = sprites[str(sprite_id)]
        x, y, w, h = sprite['atlas_rect']
        ppu = sprite['pixels_per_unit']
        border = sprite['border']['w'] / ppu
        sprite_height = sprite['source_rect']['height'] / ppu
        lines.append('{' + vector([x/width, y/height, (x+w)/width, (y+h)/height]) +
                     f', {f(border)}, {f(sprite_height)}}},')
    open_width = sprites[str(roles['sustains'][4])]['source_rect']['width'] / \
        sprites[str(roles['sustains'][4])]['pixels_per_unit'] * 5.0
    lines += ['};', f'inline constexpr float openTailHalfWidth = {f(open_width * 0.5)};']
    lines += ['} // namespace pc_guitar', '']
    return '\n'.join(lines)

if __name__ == '__main__':
    source = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT/'assets/pc_guitar/manifest.json'
    data = json.loads(source.read_text())
    output = ROOT/'examples/cloneheronative/pc_guitar_note_variants.hpp'
    output.write_text(emit(data))
    print(f'Wrote {output}')
