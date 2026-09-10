"""Check tracked files for accidental local artifacts before publication."""
from pathlib import Path
import re
import subprocess

files = subprocess.check_output(['git', 'ls-files', '-z']).decode('utf-8').split('\0')
errors = []
private_path = re.compile(rb'[A-Za-z]:[\\/](?:Users[\\/][^\\/\r\n]+|Desktop[\\/])', re.I)
for name in filter(None, files):
    path = Path(name)
    if name.startswith(('work/', 'deps/')) or path.suffix.lower() == '.ssf' or path.name in {'sogou-skin.ini', 'installation.json', 'tsf-machine-backup.json'}:
        errors.append(name + ': local artifact')
    if name.startswith(('tools/', 'tests/', 'docs/')) or name in {'README.md', 'SOGOU_SKINS.md'}:
        if path.is_file() and private_path.search(path.read_bytes()):
            errors.append(name + ': personal absolute path')
if errors:
    raise SystemExit('\n'.join(errors))
print('PASS: tracked public tree contains no detected local artifacts')
