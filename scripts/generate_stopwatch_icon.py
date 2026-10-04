"""Generate a native-size icon matching the existing rounded SVG icon set."""
from pathlib import Path
import io
import cairosvg
from PIL import Image
svg='<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="white" stroke-width="1.65" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="14" r="8"/><path d="M9 2h6m-3 0v4m0 4v4M5 6 3 4m16 2 2-2"/></svg>'
icons={'Stopwatch':svg,'Hourglass':svg[:svg.index('<circle')]+ '<path d="M6 3h12M6 21h12M7 3v4c0 2 3 3 5 5-2 2-5 3-5 5v4m10-18v4c0 2-3 3-5 5 2 2 5 3 5 5v4"/></svg>'}
header='#pragma once\n#include "DeskAssets.h"\n'
for name,source in icons.items():
    png=cairosvg.svg2png(bytestring=source.encode(),output_width=96,output_height=96)
    alpha=Image.open(io.BytesIO(png)).getchannel('A').resize((24,24),Image.Resampling.LANCZOS)
    values=[min(15,(v+8)//17) for v in alpha.tobytes()]
    packed=[(values[i]<<4)|values[i+1] for i in range(0,len(values),2)]
    rows=[','.join(map(str,packed[i:i+32]))+',' for i in range(0,len(packed),32)]
    header+=f'static const uint8_t {name}24Pixels[] PROGMEM={{\n'+'\n'.join(rows)+f'\n}};\nstatic const AAIcon Icon{name}24={{{name}24Pixels,sizeof({name}24Pixels),24}};\n'
(Path(__file__).resolve().parents[1]/'UniversalDeskDashboard/StopwatchIcon.h').write_text(header)
