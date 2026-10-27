"""Review-only refinement: shared 9px clearance, 2px external gutters.
No production GUI file is modified. Uses current native GDI drawing primitives.
"""
import json
import shutil
import subprocess
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageChops
import compact_layout_study as study

ROOT=study.ROOT
OUT=ROOT/'artifacts/continuation-review/gui'
WORK=OUT/'private-renderer'


def run():
    OUT.mkdir(parents=True,exist_ok=True);WORK.mkdir(exist_ok=True)
    study.OUT=OUT;study.WORK=WORK
    for p in (ROOT/'port').iterdir():
        if p.is_file():shutil.copyfile(p,WORK/p.name)
    for name in ('Editor.cpp','MockGeometry.h'):
        shutil.copyfile(study.BASE/name,WORK/name)
    editor=(WORK/'Editor.cpp').read_text()
    layout=(ROOT/'port/Layout.h').read_text()
    params=(ROOT/'port/ExpansionParameters.h').read_text()
    probe=study.PROBE.replace('"GLASS 07"','"GLASS 07","RASTER","CLASSIC","CLEAN","PGM"')
    (WORK/'probe.cpp').write_text(probe)
    exe=study.compile_private(True)
    production=json.loads(subprocess.check_output([str(exe)],text=True))
    study.PRODUCTION=production
    g=study.configure(False,editor,layout,params,52)
    # Existing lower-row clearance = 370 - (343 + 18) = 9.
    clearance=370-(343+18)
    top=114+18+clearance
    middle=370-154
    matrix=568-48-top-middle
    assert (clearance,top,middle,matrix)==(9,141,216,163)
    delta=top-154
    private=(WORK/'Layout.h').read_text()
    private=private.replace('TopHeight=154',f'TopHeight={top}').replace('MiddleRowBottom=370',f'MiddleRowBottom={top+middle}')
    # Junction x480 must be shared by VCO2/MIXER and EG1/EG2. Both VCOs
    # widen equally by2; their content margins increase equally by1. XMOD
    # and MIXER each donate2px, preserving the x308 oscillator translation.
    private=private.replace('{0,0,170,','{0,0,172,').replace('{170,0,138,','{172,0,136,')
    private=private.replace('{308,0,170,','{308,0,172,').replace('{478,0,104,','{480,0,102,')
    private=private.replace('OscMargin=12','OscMargin=13')
    private=private.replace('topGrid::knob(478+104/2','topGrid::knob(480+102/2')
    private=private.replace('S(MixerMode,486,topGrid::Selectors,88,','S(MixerMode,488,topGrid::Selectors,86,')
    private=private.replace('S(XMode,180,topGrid::Selectors,54,','S(XMode,182,topGrid::Selectors,52,')
    private=private.replace('TopKnobs=184, LowerKnobs=258',f'TopKnobs={184+delta}, LowerKnobs={258+delta}')
    private=private.replace('{0,MiddleRowBottom,960,150,','{0,MiddleRowBottom,960,163,')
    private=private.replace('HeaderClearance=1','HeaderClearance=3').replace('RowStep=19','RowStep=20')
    (WORK/'Layout.h').write_text(private)
    widths=production['font']['widths']
    segment_gap=1
    segment_width=(146-2*segment_gap)//3
    segments=[segment_width]*3
    segment_inset=(segment_width-max(widths[s] for s in ('RASTER','CLASSIC','CLEAN')))//2
    assert segments==[48,48,48] and sum(segments)+2*segment_gap==146
    assert segment_inset==3
    native=(WORK/'Editor.cpp').read_text()
    native=native.replace('if(mockMode() && (id==1||id==3))label="Position";',
        'if((mockMode()==1||mockMode()==2||(mockMode()==3&&id==3))&&(id==1||id==3))label="Position";')
    native=native.replace('if(mockVco&&mockMode())return;','if(mockVco)return;')
    native=native.replace('NativeWidth-4, 44','NativeWidth-4, 45')
    # True corner fasteners: identical6px horizontal AND vertical inset.
    native=native.replace('screw(p.x+6,p.y+11);screw(p.x+p.w-6,p.y+11);',
                          'screw(p.x+studygrid::ScrewInset,p.y+studygrid::ScrewInset);screw(p.x+p.w-studygrid::ScrewInset,p.y+studygrid::ScrewInset);')
    native=native.replace('screw(p.x+6,p.y+p.h-6);screw(p.x+p.w-6,p.y+p.h-6);',
                          'screw(p.x+studygrid::ScrewInset,p.y+p.h-studygrid::ScrewInset);screw(p.x+p.w-studygrid::ScrewInset,p.y+p.h-studygrid::ScrewInset);')
    begin,end=native.index('  void mockExtraVcos(){'),native.index('  void mockRulers(bool browser){')
    drawing=r'''
  void mockExtraVcos(){
    for(int o=0;o<2;++o){int x=mockgrid::Origins[o]+studygrid::Margin;
      const bool analytic=mockMode()==0||(mockMode()==3&&o==0),user=mockMode()==2&&o==1;
      if(analytic){
        studyDropdown(x,studygrid::Row1,studygrid::ContentWidth,o?"PULSE":"SAW UP");
        const char* labels[]={"RASTER","CLASSIC","CLEAN"};
        const int selected=o?2:0;int xx=x;
        for(int i=0;i<3;++i){gradient(xx,studygrid::Row2,studygrid::SegmentWidth,18,{139,153,150},{52,82,88});
          bevel(xx,studygrid::Row2,studygrid::SegmentWidth,18,i==selected);
          text(xx+studygrid::SegmentInset,studygrid::Row2,studygrid::SegmentWidth-2*studygrid::SegmentInset,18,labels[i],i==selected?RGB(226,226,212):RGB(127,215,216));xx+=studygrid::SegmentWidth+(i<2?studygrid::SegmentGap:0);}
      }else{
        studyDropdown(x,studygrid::Row1,studygrid::SourceWidth,user?"USER":"ROM");
        mockField(x+studygrid::SourceWidth+studygrid::Gap,studygrid::Row1+48,studygrid::TableWidth,18,user?"GLASS 07":o?"HORN 01":"PAD MAJ7");
        mockField(x+studygrid::ContentWidth-studygrid::BrowserWidth,studygrid::Row1+48,studygrid::BrowserWidth,18,"PGM",true);
        mockField(x,studygrid::Row2+48,studygrid::ColumnWidth,18,o?"S:LINEAR":"S:HOLD",true);
        mockField(x+studygrid::ColumnWidth+studygrid::ColumnGap,studygrid::Row2+48,studygrid::ColumnWidth,18,user?"F:STEP":"F:MORPH",true);
      }
    }
  }
'''.replace('SEGMENTS',','.join(map(str,segments)))
    native=native[:begin]+drawing+native[end:]
    (WORK/'Editor.cpp').write_text(native)
    # Generate rulers from the actual private Layout.h, not earlier prose.
    actual=json.loads(subprocess.check_output([str(study.compile_private(True))],text=True))
    xs={0,13,172,308,321,480,582,834,960,240,720}
    for origin in (0,308):
        xs.update(origin+x for x in (13,47,65,69,81,91,125,137,141,159))
        x=origin+13
        for i,w in enumerate(segments):
            xs.add(x);xs.add(x+w);x+=w+(segment_gap if i<2 else 0)
        assert x==origin+159
    ys={0,2,47,48,49,52,66,75,93,111,113,123,126,127,137,139,142,155,160,162,175,180,188,189,190,404,405,406,559,567,568}
    tm=production['font'];baseline=lambda y,h:y+(h-tm['height'])//2+tm['ascent']
    for c in actual['controls']:
        if c['control'][1]<top:continue
        for b in (c['field'],c['label_bounds']):
            y=b[1]+48;ys.update((y,y+b[3],baseline(y,b[3])))
    for name,x,y,w,height in actual['panels']:
        xs.update((x+6,x+w-6));ys.update((y+48+6,y+48+height-6))
    assert actual['panels'][2][1]+actual['panels'][2][3]==actual['panels'][3][1]==actual['panels'][8][1]==480
    knobs={c['id']:c for c in actual['controls'] if c['id'] in (0,1,2,3)}
    for a,b in ((0,2),(1,3)):
        assert knobs[b]['control']==[knobs[a]['control'][0]+308,*knobs[a]['control'][1:]]
        assert knobs[b]['field']==[knobs[a]['field'][0]+308,*knobs[a]['field'][1:]]
    h=(WORK/'StudyGeometry.h').read_text()
    h=h.replace('Margin=12','Margin=13')
    import re
    for key,values in (('RulersX',xs),('RulersY',ys)):
        h=re.sub(r'constexpr int '+key+r'\[\]=\{[^}]+\};','constexpr int '+key+'[]={'+','.join(map(str,sorted(values)))+'};',h)
    h=h.replace('BrowserWidth=18','BrowserWidth=18')
    # PGM uses the same 4px inset as table text. Source need only fit ROM/USER;
    # analytic WAVE is contextual and is not displayed inside this source field.
    lib_width=2*((widths['PGM']+8+1)//2)
    source_width=2*((max(widths[s] for s in ('ROM','USER'))+22+1)//2)
    table_width=146-source_width-8-lib_width
    assert table_width>=widths['PAD MAJ7']+8
    h=h.replace('SourceWidth=52','SourceWidth='+str(source_width)).replace('TableWidth=68','TableWidth='+str(table_width)).replace('BrowserWidth=18','BrowserWidth='+str(lib_width))
    # Definitions follow ContentWidth so the equality is compile-time checked.
    h=h.replace('\n}',f'\nconstexpr int SegmentWidth={segment_width},SegmentGap={segment_gap},SegmentInset={segment_inset},ScrewInset=6;\nstatic_assert(3*SegmentWidth+2*SegmentGap==ContentWidth);\n}}')
    # Table/button right edges follow these same new widths.
    xs.discard(137);xs.discard(141);xs.discard(445);xs.discard(449)
    for origin in (0,308):
        xs.discard(origin+65);xs.discard(origin+69)
        xs.update((origin+13+source_width,origin+13+source_width+4,origin+159-lib_width-4,origin+159-lib_width))
    h=re.sub(r'constexpr int RulersX\[\]=\{[^}]+\};','constexpr int RulersX[]={'+','.join(map(str,sorted(xs)))+'};',h)
    (WORK/'StudyGeometry.h').write_text(h)
    # The native guide crosses must follow the same +1 content translation.
    mock=(WORK/'MockGeometry.h').read_text().replace('{{46,93},{124,93},{354,93},{432,93}}','{{47,93},{125,93},{355,93},{433,93}}')
    (WORK/'MockGeometry.h').write_text(mock)
    exe=study.compile_private()
    images={name:study.capture(exe,name,mode) for name,mode in (('analytic',0),('rom',1),('mixed-rom-user',2),('mixed-analytic-rom',3))}
    for image in images.values():
        for box in ((0,0,960,48),(172,48,308,189),(480,48,960,189),(0,189,960,568)):
            assert ImageChops.difference(images['rom'].crop(box),image.crop(box)).getbbox() is None
    assert ImageChops.difference(images['analytic'].crop((0,48,172,189)),images['mixed-analytic-rom'].crop((0,48,172,189))).getbbox() is None
    assert ImageChops.difference(images['rom'].crop((308,48,480,189)),images['mixed-analytic-rom'].crop((308,48,480,189))).getbbox() is None
    images['analytic'].crop((450,159,510,219)).resize((360,360),Image.Resampling.NEAREST).save(OUT/'junction-6x.png')
    study.capture(exe,'rulers',mode=1,guides=True)
    # Label construction outside the native panel: no labels obscure controls.
    canvas=Image.new('RGB',(1300,568),'#142c32');canvas.paste(images['rom'],(0,0));d=ImageDraw.Draw(canvas)
    font=ImageFont.truetype('C:/Windows/Fonts/tahoma.ttf',11)
    notes=[(47,'Header shell ends47; top shell begins49: gutter2'),(48,'Header boundary48'),
      (93,'VCO knob centers Y93'),(142,'Selector row1 Y142, H18'),(162,'Selector row2 Y162, H18'),
      (180,'Last top control ends180; boundary189: clear9'),(189,'Top/middle boundary189; shell gutter2'),
      (396,'Middle controls end396; boundary405: clear9'),(405,'Matrix boundary405; shell gutter2'),
      (559,'Matrix controls end559; bottom568: clear9')]
    text_y=10
    for y,label in notes:
        yy=min(max(y,text_y),550);d.line((950,y,966,y,974,yy+6),fill='#e99c70');d.text((978,yy),label,font=font,fill='#e6e6d4');text_y=yy+18
    canvas.save(OUT/'vertical-rhythm.png')
    facts=dict(size=[960,568],header=[0,0,960,48],top=[0,48,960,141],middle=[0,189,960,216],matrix=[0,405,960,163],
      shell_gutters=[2,2,2],internal_clearance=[9,9,9],segmented_widths=segments,segment_gap=segment_gap,segment_text_inset=segment_inset,segment_y=162,segment_height=18,
      source_width=source_width,table_width=table_width,programmer_width=lib_width,field_gap=4,margin=13,vco_width=172,screw_inset=6,
      junction=[480,189],junction_screw_centers=[[474,183],[486,183],[474,195],[486,195]],
      equation=f'13 +{source_width} +4 +{table_width} +4 +{lib_width} +13 =172',rulers_x=sorted(xs),rulers_y=sorted(ys),
      production_geometry=production,proposed_geometry=actual)
    (OUT/'geometry.json').write_text(json.dumps(facts,indent=2))
    (OUT/'VALIDATION.json').write_text(json.dumps(dict(passed=True,equal_segments=[48,48,48],segment_gaps=[1,1],group_width=146,
        vco_translation=308,aligned_junction=[480,189],junction_square_side=12,screw_diameter=6,screw_inset=6,
        non_vco_state_images_pixel_identical=True,analytic_rom_contexts_pixel_identical=True),indent=2))
    report=f'''# Final proposed GUI geometry — REVIEW ONLY

Primary size960×568. No production Editor/Layout, theme, parameter or state changes.
The former [...] / Lib button is now **PGM**, solely ROM PROGRAMMER / WAVEFORM LIBRARIAN.
No analytic quality controls live behind PGM. It is absent in analytic mode.

## Shared rules and exact bands (client coordinates, half-open)

| Region | Bounds X,Y,W,H | Status |
|---|---|---|
| Header logical allocation |0,0,960,48|EXISTING CONSTANT|
| Header visible shell |2,2,956,45|NEW PROPOSED SHARED CONSTANT: height45 instead of44|
| Top logical row |0,48,960,141|NEW PROPOSED SHARED CONSTANT|
| Middle logical row |0,189,960,216|EXISTING HEIGHT, translated−13|
| Matrix logical row |0,405,960,163|NEW PROPOSED SHARED CONSTANT|

Logical bands touch (external logical gap0). Visible shell gaps are **2/2/2px**:
header end47 →top start49; top end188 →middle start190; middle end404 →matrix start406.
The old header gap was3px, whereas the other shell gaps were2px. This corrects that discrepancy.
Internal last-control-to-logical-boundary clearance is **9/9/9px**. To visible shell it is8px.
Existing middle9=370−(343+18) is the canonical ruler. Top height=114+18+9=141.
Matrix height=36+5×20+18+9=163; matrix first-field offset36=4+14+3+12+3.
Matrix row pitch20=existing selector18+existing inter-row gap2; header clearance3=old1+2.
This redistributes B's excess top space into a deliberate matrix rhythm, without changing overall height.
No smaller alternative is included: shrinking further would break the shared9px clearance or this complete568px allocation.

## VCO identity and fields

VCO modules172×141 (NEW SHARED WIDTH); VCO2=VCO1+308X EXACTLY. NEW margin13, EXISTING content146,
columns68+10+68, knobs36px; centers(47,93)/(125,93) and(355,93)/(433,93).
Label boxesY113 H13; glyph baseline123. Readouts(20,127,54,12)/(98,127,54,12),
translated+308 for VCO2; glyph baseline137. These are EXISTING CONSTANTS.
NEW PROPOSED rowsY142/162 H18 pitch20, text baseline155/175.

ROM/USER row1: {facts['equation']}. Table X{13+source_width+4} W{table_width}; PGM X{159-lib_width} W{lib_width}; source X13 W{source_width}.
PGM width is derived from actual GDI Tahoma10px extent({widths['PGM']})+2×4px insets rounded to an even pixel.
Source width derives from max(ROM,USER)+2×11px inherited arrow gutters, rounded even.
WAVE is not shown in this contextual field; analytic uses its own full-width wave selector.
The changed button transfers width from TABLE, never shrinks font or breaks margins.
Row2:13+68+10+68+13=172; SAMPLE X13, FRAME X91. Both align exactly with knob centers.
ROM and USER use identical rectangles, differing only in static content.

Analytic WAVE: X13,Y142,W146,H18; no ROM table/PGM/interpolation controls.
One mutually-exclusive RASTER/CLASSIC/CLEAN segmented group: X13,Y162,W146,H18;
cell widths{segments}, separated by two identical1px gaps.
48+1+48+1+48=146 exactly. All three text boxes use the same3px inset, derived
from(48−max measured glyph width42)/2. Font size is unchanged. VCO2 adds308X.
The group spans both knob columns, belongs independently to each VCO, and is not three toggles.
VCO1 is shown RASTER andVCO2 CLEAN to demonstrate independent proposed state.
RASTER remains conceptual: no production encoding/behavior implied.

## Four-corner junction correction

Old top seam478 versus lower seam480 caused the visible stagger. NEW shared seam480.
Both VCOs widen170→172; XMOD138→136 and MIXER104→102. VCO2 origin308 is unchanged;
both VCO content blocks translate+1px, preserving identical13px margins and146px content.
No independent VCO adjustment exists. Other module boundary X coordinates are unchanged.
Every module screw retains the original6px diameter and now uses6px X/Y corner inset.
At the corrected junction(480,189), centers are(474,183),(486,183),(474,195),(486,195):
an exact12×12 square, with equal2px horizontal/vertical shell gutters.
The former top11px/bottom6px screw offsets are no longer mixed.

All new geometry is PROPOSED. Current production knob/fonts/header art/colors and unrelated controls remain the authority.
Full X/Y rulers and actual measured pre/post controls are in geometry.json. All overlays derive from those coordinates.
The rhythm overlay's labelled annotation rail is outside the960px GUI; it is not part of the proposed window.
'''
    (OUT/'GEOMETRY.md').write_text(report,encoding='utf-8')
    print('Four native previews + rulers + labelled rhythm overlay complete. PGM width',lib_width,'table width',table_width)


if __name__=='__main__':run()
