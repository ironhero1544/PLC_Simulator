"""Apply display-only groups without changing the exam's layout or sequence."""

import json
from datetime import datetime, timezone
from pathlib import Path
from zipfile import ZipFile


GROUPS = {
    '01 제어 · 전원': {0, 1, 2, 3, 4},
    '02 공압 공급': {5, 6, 7},
    '03 공작물 공급': {10, 23, 30},
    '04 송출 · 가공': {11, 12, 31, 32, 43, 44, 45},
    '05 컨베이어 · 분류 · 스토퍼': {13, 17, 18, 22, 24, 25, 33, 46, 47, 48},
    '06 리프트 · 흡착': {14, 21, 34, 36},
    '07 창고 · 저장박스': {16, 20, 35},
    '08 공작물': {50, 51, 52, 53},
}


def apply_render_groups(layout):
    for component in layout['components']:
        matching = [name for name, ids in GROUPS.items()
                    if component['instance_id'] in ids]
        if len(matching) != 1:
            raise ValueError(f"Unmapped component: {component['instance_id']}")
        layer = component.setdefault('render_layer', {})
        layer.setdefault('opacity', 1.0)
        layer.setdefault('hidden', False)
        layer['group'] = matching[0]


def strip_display_metadata(layout):
    copy = json.loads(json.dumps(layout))
    for component in copy['components']:
        component.pop('render_layer', None)
    return copy


def grouped_text(original):
    layout = json.loads(original)
    before = strip_display_metadata(layout)
    apply_render_groups(layout)
    assert strip_display_metadata(layout) == before
    return json.dumps(layout, ensure_ascii=False, indent=2)


def msvc_checksum(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & 0xffffffffffffffff
    return str(value)


def update_examples():
    folder = Path(__file__).resolve().parent
    for name in ('answer', 'continuous'):
        path = folder / f'automation-exam-03-{name}.json'
        path.write_text(grouped_text(path.read_text(encoding='utf-8-sig')),
                        encoding='utf-8')

    path = folder / 'automation-exam-03.plcproj'
    with ZipFile(path) as archive:
        entries = [(info, archive.read(info.filename))
                   for info in archive.infolist()]
    original = dict((info.filename, data) for info, data in entries)
    layout_data = grouped_text(original['layout.json']).encode('utf-8')
    manifest = json.loads(original['manifest.json'])
    manifest['layoutChecksum'] = msvc_checksum(layout_data)
    manifest['lastModified'] = datetime.now(timezone.utc).strftime(
        '%Y-%m-%dT%H:%M:%SZ')
    updated = {
        'layout.json': layout_data,
        'manifest.json': json.dumps(manifest, ensure_ascii=False,
                                    indent=2).encode('utf-8'),
    }
    temporary = path.with_suffix('.grouping.tmp')
    with ZipFile(temporary, 'w') as archive:
        for info, data in entries:
            archive.writestr(info, updated.get(info.filename, data))
    with ZipFile(temporary) as archive:
        assert archive.testzip() is None
        for name, data in original.items():
            if name not in updated:
                assert archive.read(name) == data, name
        assert json.loads(archive.read('manifest.json'))['layoutChecksum'] == (
            msvc_checksum(archive.read('layout.json')))
    assert path.resolve().is_relative_to(folder)
    assert temporary.resolve().is_relative_to(folder)
    temporary.replace(path)
    print(f'Grouped {len(json.loads(layout_data)["components"])} components '
          f'into {len(GROUPS)} groups. Layout, wiring and program preserved.')


if __name__ == '__main__':
    update_examples()
