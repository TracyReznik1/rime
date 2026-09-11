"""Retain the notices supplied with the runtimes bundled by PyInstaller."""
import importlib.metadata
from pathlib import Path
import shutil
import sys

target = Path(sys.argv[1]) / 'notices'
target.mkdir(parents=True, exist_ok=True)
for name in ('Pillow', 'PyInstaller'):
    distribution = importlib.metadata.distribution(name)
    files = [entry for entry in distribution.files or [] if '/licenses/' in str(entry).replace('\\', '/')]
    if not files:
        raise RuntimeError(f'Missing installed license files for {name}')
    for entry in files:
        destination = target / name / Path(str(entry).split('licenses/', 1)[-1])
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(distribution.locate_file(entry), destination)
shutil.copyfile(Path(sys.base_prefix) / 'LICENSE.txt', target / 'Python-LICENSE.txt')
tk_notice = Path(sys.argv[1]) / 'manager-runtime' / '_tk_data' / 'license.terms'
if not tk_notice.is_file():
    raise RuntimeError('Bundled Tk license is missing')
shutil.copyfile(tk_notice, target / 'Tk-license.terms')
(target / 'README.txt').write_text(
    'This manager bundles CPython, Pillow, Tcl/Tk and the PyInstaller bootloader.\n'
    'Their supplied notices are retained here and in the runtime directory.\n'
    'Pillow LICENSE includes its bundled third-party dependency notices.\n', encoding='utf-8')
print('Bundled runtime notices collected.')
