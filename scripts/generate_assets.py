"""Generate native-size antialiased Inter glyphs and rounded line icons.
Usage: python scripts/generate_assets.py --font Inter.ttf
Font: https://github.com/google/fonts/tree/main/ofl/inter (SIL OFL, design/Inter-OFL.txt).
Requires Pillow and CairoSVG. Assets use packed four-bit alpha coverage to fit the ESP32 app partition.
"""
from pathlib import Path
import argparse, io, math
from PIL import Image, ImageDraw, ImageFont
import cairosvg
p=argparse.ArgumentParser();p.add_argument('--font',required=True);args=p.parse_args()
root=Path(__file__).resolve().parents[1]
out=['#pragma once','#include <stdint.h>','#ifndef PROGMEM','#define PROGMEM','#endif',
     'struct AAGlyph { uint16_t code; uint8_t w,h,advance; int8_t left,top; uint32_t offset; uint16_t size; };',
     'struct AAFont { const AAGlyph* glyphs; const uint8_t* data; uint16_t count; };',
     'struct AAIcon { const uint8_t* data; uint16_t size; uint8_t width; };']
def pack(b):
    # Four-bit coverage gives 16 antialiasing levels, compact enough for CYD flash.
    values=[min(15,(v+8)//17) for v in b]
    if len(values)%2:values.append(0)
    return [(values[i]<<4)|values[i+1] for i in range(0,len(values),2)]

def array(name,b):
    out.append(f'static const uint8_t {name}[] PROGMEM={{')
    for i in range(0,len(b),32):out.append(','.join(map(str,b[i:i+32]))+',')
    out.append('};')

for name,size,weight,chars in [('Small',11,450,''),('Body',14,450,''),('Title',18,600,''),('Metric',24,600,'0123456789.-kM%°'),('Focus',64,550,'0123456789:-'),('Clock',96,550,'0123456789:-')]:
    if not chars:chars=''.join(map(chr,range(32,127)))+'°·›áčďéěíňóřšťúůýžÁČĎÉĚÍŇÓŘŠŤÚŮÝŽ'
    font=ImageFont.truetype(args.font,size*4)
    axes=font.get_variation_axes();font.set_variation_by_axes([min(max(24,a['minimum']),a['maximum']) if a['name']==b'Optical size' else weight for a in axes])
    data=[];glyphs=[]
    for c in chars:
        box=font.getbbox(c,anchor='ls');left=math.floor(box[0]/4);top=math.floor(box[1]/4)
        w=max(0,math.ceil(box[2]/4)-left);h=max(0,math.ceil(box[3]/4)-top)
        advance=round(font.getlength(c)/4)
        if w and h:
            image=Image.new('L',(w*4,h*4));ImageDraw.Draw(image).text((-left*4,-top*4),c,font=font,fill=255,anchor='ls')
            mask=image.resize((w,h),Image.Resampling.LANCZOS);packed=pack(mask.tobytes())
        else:packed=[]
        glyphs.append((ord(c),w,h,advance,left,top,len(data),len(packed)));data+=packed
    array(name+'Pixels',data)
    out.append(f'static const AAGlyph {name}Glyphs[] PROGMEM={{')
    out+=['{'+','.join(map(str,g))+'},' for g in glyphs];out.append('};')
    out.append(f'static const AAFont Font{name}={{ {name}Glyphs,{name}Pixels,{len(glyphs)} }};')

icons={
'Weather':'<circle cx="8" cy="8" r="3"/><path d="M8 1v2m-7 5h2m.8-4.2 1.4 1.4M12.2 3.8l-1.4 1.4"/><path d="M8 19a4 4 0 0 1-.2-8 5 5 0 0 1 9.5 2A3 3 0 0 1 18 19Z"/>',
'Focus':'<circle cx="12" cy="13" r="8"/><path d="M9 2h6m-3 0v3m0 4v4l3 2M18 5l2-2"/>',
'Stats':'<path d="M4 3v17h17M7 14l4-4 4 2 5-7m-4 0h4v4"/>',
'Settings':'<path d="M4 6h16M4 12h16M4 18h16"/><rect x="7" y="3.5" width="4" height="5" rx="1.5" fill="white"/><rect x="14" y="9.5" width="4" height="5" rx="1.5" fill="white"/><rect x="8" y="15.5" width="4" height="5" rx="1.5" fill="white"/>',
'Printer':'<path d="M7 8V3h10v5M7 17H4V9h16v8h-3"/><path d="M7 14h10v7H7zM16 11h1"/>',
'Download':'<path d="M12 3v12m-5-5 5 5 5-5M4 17v4h16v-4"/>',
'Heart':'<path d="M12 20 4 12a5 5 0 0 1 8-6 5 5 0 0 1 8 6Z"/>',
'People':'<circle cx="9" cy="7" r="3"/><path d="M3 20v-2a6 6 0 0 1 12 0v2M16 4a3 3 0 0 1 0 6m2 4a5 5 0 0 1 3 4v2"/>',
'Wifi':'<path d="M3 8a14 14 0 0 1 18 0M6 12a9 9 0 0 1 12 0m-9 4a4 4 0 0 1 6 0"/><circle cx="12" cy="20" r=".6"/>',
'Sun':'<circle cx="12" cy="12" r="4"/><path d="M12 2v2m0 16v2M2 12h2m16 0h2M5 5l1.5 1.5m11 11L19 19M5 19l1.5-1.5m11-11L19 5"/>',
'Web':'<rect x="5" y="2" width="14" height="20" rx="3"/><circle cx="12" cy="11" r="4"/><path d="M8 11h8M12 7v8m-2 4h4"/>',
'Back':'<path d="m15 5-7 7 7 7"/>',
'Play':'<path d="m8 4 12 8-12 8Z"/>',
'Pause':'<path d="M8 5v14M16 5v14"/>',
'Reset':'<path d="M4 9a8 8 0 1 1 0 7M4 3v6h6"/>',
'Cloud':'<path d="M7 19a5 5 0 0 1-.5-10 6 6 0 0 1 11.5 2A4 4 0 0 1 18 19Z"/>',
'Rain':'<path d="M6 15a4 4 0 0 1 0-8 5 5 0 0 1 10 2 3 3 0 0 1 1 6M8 18l-1 3m6-3-1 3m6-3-1 3"/>',
'Snow':'<path d="M6 15a4 4 0 0 1 0-8 5 5 0 0 1 10 2 3 3 0 0 1 1 6M7 19h2m-1-1v2m5-1h2m-1-1v2m5-1h2m-1-1v2"/>',
'Storm':'<path d="M6 15a4 4 0 0 1 0-8 5 5 0 0 1 10 2 3 3 0 0 1 1 6m-5-2-3 5h4l-3 4"/>',
'Wind':'<path d="M3 7h10a3 3 0 1 0-3-3M3 12h16a3 3 0 1 1-3 3M3 17h6a3 3 0 1 1-3 3"/>',
'Fog':'<path d="M6 12a4 4 0 0 1 0-8 5 5 0 0 1 10 2 3 3 0 0 1 1 6M4 16h16M7 20h10"/>',
}
for name,svg in icons.items():
    for size in (([16,24,44] if name=='Weather' else [24,44]) if name in ['Weather','Focus','Stats','Settings'] else ([16,24] if name in ['Sun','Weather','Cloud','Rain','Snow','Storm','Fog','Wind'] else [24])):
        body=f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="white" stroke-width="1.65" stroke-linecap="round" stroke-linejoin="round">{svg}</svg>'
        rendered=cairosvg.svg2png(bytestring=body.encode(),output_width=size*4,output_height=size*4)
        mask=Image.open(io.BytesIO(rendered)).getchannel('A').resize((size,size),Image.Resampling.LANCZOS)
        packed=pack(mask.tobytes());array(f'{name}{size}Pixels',packed)
        out.append(f'static const AAIcon Icon{name}{size}={{ {name}{size}Pixels,{len(packed)},{size} }};')
header=root/'UniversalDeskDashboard/DeskAssets.h';header.write_text('\n'.join(out)+'\n')
print(f'Generated {header.name}: {header.stat().st_size} source bytes')
