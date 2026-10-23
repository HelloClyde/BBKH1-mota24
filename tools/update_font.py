"""Add missing 12px UI glyphs; keep the existing 9588 glyphs byte-for-byte."""
import argparse
from pathlib import Path
import re
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser()
ap.add_argument('--font',type=Path,help='Explicit 12px source font; verify its redistribution terms before adding glyphs')
args=ap.parse_args()
p=ROOT/'src/font.h'
s=p.read_text('utf8')
rows={int(code):body for code,body in re.findall(r'\{(\d+),\{([^}]+)\}\}',s)}
source=''.join(f.read_text('utf8') for f in (ROOT/'src').glob('*') if f.suffix in ['.c','.h'] and f.name not in ['font.h','sprites.h'])
# Only string literals are displayed. Comments do not need bitmap glyphs.
strings=''.join(re.findall(r'"(?:\\.|[^"\\])*"',source))
missing=sorted({ord(c) for c in strings if ord(c)>127}-rows.keys())
if missing:
    if args.font is None:
        ap.error('Missing glyphs require --font PATH. Existing glyphs were generated from SimSun; see SOURCES.md for license limitations.')
    font=ImageFont.truetype(str(args.font),12)
    for code in missing:
        im=Image.new('1',(12,12));ImageDraw.Draw(im).text((0,-1),chr(code),font=font,fill=1)
        rows[code]=','.join(str(sum((1<<(11-x)) for x in range(12) if im.getpixel((x,y)))) for y in range(12))
    p.write_text('typedef struct { unsigned int code; unsigned short rows[12]; } Glyph;\nstatic const Glyph glyphs[] = {\n'+''.join('{%d,{%s}},\n'%(code,body) for code,body in sorted(rows.items()))+'};\n',encoding='utf8')
print(f'Added {len(missing)} glyphs: '+''.join(chr(c) for c in missing))
