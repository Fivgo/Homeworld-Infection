#!/usr/bin/env python3
"""Write src/Game/AssetsExportReadme.h from export_assets.README_TEXT (HWI-FEATURE(assets-autoexport)).

The engine's export (src/Game/AssetsExport.c) writes the Assets folder's README.txt byte for byte as
tools/assets/export_assets.py does; this keeps the two texts one. Run it after changing README_TEXT:
  python3 tools/assets/gen_readme_header.py          write the header
  python3 tools/assets/gen_readme_header.py --check  exit 1 when the header is out of date
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.dirname(HERE))
import export_assets  # noqa: E402

OUT = os.path.join(HERE, '..', '..', 'src', 'Game', 'AssetsExportReadme.h')


def header_text():
    text = export_assets.README_TEXT
    if not text.isascii():
        raise SystemExit('README_TEXT must be ASCII')
    lines = ['/* HWI-FEATURE(assets-autoexport): README.txt of the Assets folder, generated from',
             '   tools/assets/export_assets.py README_TEXT by tools/assets/gen_readme_header.py. Do not edit: change the',
             '   Python text and run the script. */',
             '',
             'static const char aeReadmeText[] =']
    for line in text.splitlines(True):
        esc = line.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n')
        lines.append('    "%s"' % esc)
    lines[-1] += ';'
    return '\n'.join(lines) + '\n'


def main():
    text = header_text()
    if '--check' in sys.argv[1:]:
        with open(OUT) as fh:
            current = fh.read()
        if current != text:
            print('%s is out of date: run tools/assets/gen_readme_header.py' % os.path.normpath(OUT))
            return 1
        print('%s is current' % os.path.normpath(OUT))
        return 0
    with open(OUT, 'w', newline='\n') as fh:
        fh.write(text)
    print('wrote %s' % os.path.normpath(OUT))
    return 0


if __name__ == '__main__':
    sys.exit(main())
