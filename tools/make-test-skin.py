"""Generate an original geometric fixture; no third-party skin artwork."""
import argparse
import io
from pathlib import Path
import zipfile
from PIL import Image, ImageDraw
from import_sogou_skin import import_skin, activate

parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
image = Image.new('RGBA', (180, 90))
draw = ImageDraw.Draw(image)
draw.rounded_rectangle((4, 4, 175, 85), radius=12, fill=(245, 225, 235, 240), outline=(170, 90, 130, 255), width=2)
buffer = io.BytesIO()
image.save(buffer, format='PNG')
with zipfile.ZipFile(args.output / 'geometric.ssf', 'w') as archive:
    archive.writestr('bg.png', buffer.getvalue())
    archive.writestr('skin.ini', '[General]\nskin_name=Geometric\n[Display]\nfont_size=16\n[Scheme_H1]\npic=bg.png\nlayout_horizontal=0,20,20\nlayout_vertical=0,20,20\npinyin_marge=10,8,10,8\nzhongwen_marge=10,8,10,8\n')
directory, _ = import_skin(args.output / 'geometric.ssf', args.output / 'skin')
activate(directory, args.output)
(args.output / 'sogou-skin.ini').rename(args.output / 'active.ini')
