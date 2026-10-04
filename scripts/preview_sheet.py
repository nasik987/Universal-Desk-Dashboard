"""Compose the native renderer output; all screen pixels come from tests/render_ui.cpp."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
root=Path(__file__).resolve().parents[1]
names=['Clock','Apps','Weather','Focus','MakerWorld','Settings','Stopwatch']
sheet=Image.new('RGB',(1000,823),'#090a0c');draw=ImageDraw.Draw(sheet)
font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',14)
for i,name in enumerate(names):
    frame=Image.open(root/'tests/previews'/('stopwatch.ppm' if name=='Stopwatch' else f'page-{i}.ppm'))
    x=12+i%3*330;y=26+i//3*263;sheet.paste(frame,(x,y))
    draw.text((x,y-20),name,font=font,fill='#959aa5')
draw.text((12,803),'Dark theme · software rendering · sample screen data · native resolution 320 × 240',font=font,fill='#959aa5')
sheet.save(root/'design/UI-preview.png')
settings=Image.new('RGB',(680,560),'#090a0c');draw=ImageDraw.Draw(settings)
for i,name in enumerate(['Graphics','Display','Time','Weather']):
    frame=Image.open(root/f'tests/previews/settings-{name.lower()}.ppm')
    x=12+i%2*330;y=26+i//2*263;settings.paste(frame,(x,y))
    draw.text((x,y-20),name,font=font,fill='#959aa5')
draw.text((12,540),'Production renderer · on-device settings · sample data',font=font,fill='#959aa5')
settings.save(root/'design/Settings-preview.png')
