"""Non-production, native-GDI layout comparisons. Never used by plugin targets.

Run with the bundled Python. Only private renderer copies and study artifacts
are written; factory assets, production sources and existing previews are read.
"""
from pathlib import Path
import hashlib
import json
import os
import re
import shutil
import subprocess
from PIL import Image, ImageChops
import gui_mockups as prior

ROOT = prior.ROOT
OUT = ROOT / 'artifacts/gui-compact-study'
WORK = OUT / 'private-renderer'
BASE = ROOT / 'artifacts/factory-rom/gui-mockup-source'
VS = Path(r'C:\Program Files\Microsoft Visual Studio\18\Community')


def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def boundary():
    files = [p for folder in ('port', 'factory', 'presets', 'docs')
             for p in (ROOT / folder).rglob('*') if p.is_file()]
    files += list(ROOT.glob('CMakeLists.txt'))
    return {str(p.relative_to(ROOT)): digest(p) for p in files}


def write_json(name, data):
    (OUT / name).write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')


def compile_private(probe=False):
    flags = '/nologo /std:c++17 /EHsc /O2 /MD /DNOMINMAX /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN'
    sources = f'"{WORK / "probe.cpp"}"' if probe else f'"{WORK / "Editor.cpp"}" "{WORK / "Preview.cpp"}"'
    exe = WORK / ('probe.exe' if probe else 'study.exe')
    subsystem = 'CONSOLE' if probe else 'WINDOWS'
    cmd = f'''@echo off
call "{VS / 'VC/Auxiliary/Build/vcvars64.bat'}" >nul
if errorlevel 1 exit /b %errorlevel%
cl {flags} /I"{WORK}" {sources} /Fo"{WORK}/" /Fe"{exe}" /link /SUBSYSTEM:{subsystem} gdiplus.lib comdlg32.lib ole32.lib shell32.lib gdi32.lib user32.lib advapi32.lib
exit /b %errorlevel%
'''
    (WORK / 'build.cmd').write_text(cmd)
    result = subprocess.run(['cmd.exe', '/d', '/c', str(WORK / 'build.cmd')], capture_output=True, text=True)
    with (OUT / 'private-build.log').open('a') as log:
        log.write(result.stdout + result.stderr)
    result.check_returncode()
    return exe


PROBE = r'''
#include "Layout.h"
#include <windows.h>
#include <cstdio>
int main(){
  using namespace xs;
  printf("{\"size\":[%d,%d],\"top_height\":%d,\"middle_bottom\":%d,\"panels\":[",NativeWidth,NativeHeight,faceGrid::TopHeight,MiddleRowBottom);
  bool first=true; for(auto p:panels){if(!first)printf(",");first=false;printf("[\"%s\",%d,%d,%d,%d]",p.label,p.x,p.y,p.w,p.h);}
  printf("],\"controls\":[");first=true;
  for(auto c:mainControls){if(!first)printf(",");first=false;auto f=fieldBounds(c),l=labelBounds(c);
    printf("{\"id\":%d,\"label\":\"%s\",\"kind\":%d,\"control\":[%d,%d,%d],\"field\":[%d,%d,%d,%d],\"label_bounds\":[%d,%d,%d,%d]}",c.id,c.label,int(c.kind),c.x,c.y,c.w,f.x,f.y,f.w,f.h,l.x,l.y,l.w,l.h);}
  HDC dc=CreateCompatibleDC(nullptr);auto f=CreateFontW(-10,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,DEFAULT_PITCH,L"Tahoma");auto old=SelectObject(dc,f);
  TEXTMETRICA tm{};GetTextMetricsA(dc,&tm);
  printf("],\"font\":{\"height\":%d,\"ascent\":%d,\"widths\":{",tm.tmHeight,tm.tmAscent);
  first=true;for(auto s:{"WAVE","ROM","USER","PAD MAJ7","S:LINEAR","F:MORPH","GLASS 07"}){SIZE z{};GetTextExtentPoint32A(dc,s,int(strlen(s)),&z);if(!first)printf(",");first=false;printf("\"%s\":%ld",s,z.cx);}
  printf("}}}");SelectObject(dc,old);DeleteObject(f);DeleteDC(dc);
}
'''


COMPACT = r'''
  void studyDropdown(int x,int y,int w,const char* value){
    well(x,y,w,moduleGrid::SelectorHeight);
    text(x+moduleGrid::SelectorTextInset,y+2,w-2*moduleGrid::SelectorTextInset,moduleGrid::SelectorHeight-4,value,RGB(127,215,216));
    line(x+w-8,y+7,x+w-5,y+10,RGB(127,215,216));
    line(x+w-5,y+10,x+w-2,y+7,RGB(127,215,216));
  }
  void mockExtraVcos(){
    if(!mockMode())return;
    for(int o=0;o<2;++o){const int x=mockgrid::Origins[o]+studygrid::Margin;
      const bool user=mockMode()==2&&o==1;
      studyDropdown(x,studygrid::Row1,studygrid::SourceWidth,user?"USER":"ROM");
      mockField(x+studygrid::SourceWidth+studygrid::Gap,studygrid::Row1+48,studygrid::TableWidth,18,user?"GLASS 07":o?"HORN 01":"PAD MAJ7");
      mockField(x+studygrid::ContentWidth-studygrid::BrowserWidth,studygrid::Row1+48,studygrid::BrowserWidth,18,"...",true);
      mockField(x,studygrid::Row2+48,studygrid::ColumnWidth,18,o?"S:LINEAR":"S:HOLD",true);
      mockField(x+studygrid::ColumnWidth+studygrid::ColumnGap,studygrid::Row2+48,studygrid::ColumnWidth,18,user?"F:STEP":"F:MORPH",true);
    }
  }
'''


def capture(exe, name, mode=1, guides=False):
    env = dict(os.environ, XS_ROM_MOCK_MODE=str(mode))
    for k in ('XS_ROM_MOCK_BROWSER', 'XS_ROM_MOCK_GUIDES'):
        env.pop(k, None)
    if guides:
        env['XS_ROM_MOCK_GUIDES'] = '1'
    path = OUT / (name + '.png')
    subprocess.run([str(exe), '--snapshot', str(path), '100', '--init', '--theme', 'STEEL'], env=env, check=True)
    return Image.open(path).convert('RGB')


def shifted_rect(rect, dx=0, dy=48):
    return [rect[0]+dx, rect[1]+dy, *rect[2:]]


def configure(short, original_editor, original_layout, original_params, source_width):
    delta = 20 if short else 0
    # Only private copies. Whole middle band and matrix translate by one row pitch.
    layout = original_layout.replace('TopHeight=154', f'TopHeight={154-delta}')
    layout = layout.replace('MiddleRowBottom=370', f'MiddleRowBottom={370-delta}')
    layout = layout.replace('TopKnobs=184, LowerKnobs=258', f'TopKnobs={184-delta}, LowerKnobs={258-delta}')
    (WORK / 'Layout.h').write_text(layout)
    (WORK / 'ExpansionParameters.h').write_text(original_params.replace('NativeHeight=568', f'NativeHeight={568-delta}'))
    table_width = 146-source_width-2*4-18
    x_rulers = {0,170,308,478,582,834,960,240,480,720}
    for origin in (0,308):
        for x in (12,46,80,90,124,158,12+source_width,12+source_width+4,140-4,140):
            x_rulers.add(origin+x)
    # Derived glyph baselines use actual GDI font metrics from the probe.
    font = PRODUCTION['font']
    baseline = lambda y,h: y+(h-font['height'])//2+font['ascent']
    y_rulers = {48,52,66,75,93,111,113,126,127,139,142,160,162,180,202-delta,418-delta,568-delta,
                baseline(113,13),baseline(127,12),baseline(144,14),baseline(162,18)}
    for c in PRODUCTION['controls']:
        if c['control'][1] >= 154:
            for b in (c['field'],c['label_bounds']):
                y = b[1]+48-delta
                y_rulers.update((y,y+b[3],baseline(y,b[3])))
    h = '#pragma once\nnamespace studygrid {\n'
    h += f'constexpr int Margin=12,ContentWidth=146,SourceWidth={source_width},TableWidth={table_width},Gap=4,BrowserWidth=18,ColumnWidth=68,ColumnGap=10,Row1=94,Row2=114;\n'
    h += 'constexpr int RulersX[]={'+','.join(map(str, sorted(x_rulers)))+'};\n'
    h += 'constexpr int RulersY[]={'+','.join(map(str, sorted(y_rulers)))+'};\n}\n'
    (WORK/'StudyGeometry.h').write_text(h)
    editor = original_editor.replace('#include "MockGeometry.h"', '#include "MockGeometry.h"\n#include "StudyGeometry.h"')
    # Freeze the underlay's reference ramp so a shorter canvas cannot restyle
    # the unchanged header. New lower-band gaps sample this same authored ramp.
    editor = editor.replace('gradient(0,-48,NativeWidth,NativeHeight,', 'gradient(0,-48,NativeWidth,568,')
    # Analytic mode uses the untouched production Wave/Engine controls and labels.
    # ROM/USER projection is drawn separately, so its dormant engine is never painted.
    editor = editor.replace('if(mockVco){c.y=mockgrid::SourceY;c.label="";}', 'if(mockVco&&mockMode())return;')
    editor = editor.replace('if(mockVco&&(c.id==24||c.id==25))dropdown=true;', '')
    start, end = editor.index('  void mockExtraVcos(){'), editor.index('  void mockRulers(bool browser){')
    editor = editor[:start] + COMPACT + editor[end:]
    editor = editor.replace('h=browser?384:568;', f'h=browser?384:{568-delta};')
    editor = editor.replace('mockgrid::MainRulersX', 'studygrid::RulersX').replace('mockgrid::MainRulersY', 'studygrid::RulersY')
    (WORK / 'Editor.cpp').write_text(editor)
    fields = dict(source=[12,142,source_width,18],table=[12+source_width+4,142,table_width,18],browser=[140,142,18,18],
                  sample=[12,162,68,18],frame=[90,162,68,18])
    return dict(size=[960,568-delta],top_band_height=154-delta,vco_dimensions=[170,154-delta],
                content_width=146,margin=12,gap=4,column_gap=10,row_pitch=20,
                fields_vco1=fields,fields_vco2={k:shifted_rect(v,308,0) for k,v in fields.items()},
                knob_centers=[[46,93],[124,93],[354,93],[432,93]],readouts=[[19,127,54,12],[97,127,54,12],[327,127,54,12],[405,127,54,12]],
                row_y_client=[142,162],middle_origin_client=202-delta,matrix_origin_client=418-delta,
                rulers_x=sorted(x_rulers),rulers_y=sorted(y_rulers),
                width_equation=f'12 + {source_width} + 4 + {table_width} + 4 + 18 + 12 = 170',
                interpolation_equation='12 + 68 + 10 + 68 + 12 = 170')


def equal_crop(a,b,box):
    return ImageChops.difference(a.crop(box),b.crop(box)).getbbox() is None


def report(g, source_width, checks):
    lines = ['# Xsynth 2000 — compact VCO layout study', '',
      'PREVIEW ONLY. No production implementation or layout approval is implied. All images use STEEL at native 100% size.', '',
      '## Authority and coordinate systems', '',
      'Inspected current port/Layout.h, Editor.cpp, ExpansionParameters.h and Preview.cpp, plus the completed factory-rom-review/gui-mockups/main/rom.png and generator.',
      'The production layout and previous mockup agree: 960×568, 48px header, 154px top band, middle band216px, matrix150px. Production Layout.h coordinates are body-local; every rectangle below is CLIENT-local (add48 to production Y).',
      'The previous mockup adds three ROM selector rows at client142/162/182. These are preview additions, not current production ROM controls. Production contains no ROM implementation.', '',
      '## Comparison', '',
      '| Variant | Overall | Top band | ROM rows | Visible inactive controls | Advantage | Drawback |',
      '|---|---|---:|---:|---|---|---|',
      '| A BASELINE | 960×568 | 154 | 3 | CLASSIC/CLEAN | Wide table field124px | Inactive field consumes space |',
      '| B COMPACT VCO | 960×568 | 154 | 2 | None | All other geometry unchanged | Table field narrower;22px below fields |',
      '| C COMPACT + SHORTER | 960×548 | 134 | 2 | None | Saves20px;shared lower control ruler |2px lower clearance; narrower table field |',
      '| D MIXED (C geometry) | 960×548 | 134 | 2 | None | Shows distinct ROM/USER state | Long user names need browser/full-name disclosure |',
      '| E ANALYTIC (C geometry) | 960×548 | 134 |0 | None | Original Wave/Engine retained | Contextual empty space above selectors |', '',
      'D and E use C solely to show both contexts in the shorter geometry; neither B nor C is approved or selected as the final layout.', '',
      '## Shared horizontal construction', '',
      f'Row1: 12 + {source_width} + 4 + {146-source_width-26} + 4 + 18 + 12 =170. Interior=146 exactly.',
      'Row2:12 +68 +10 +68 +12 =170. Both field centers equal the inherited knob centers46/124.',
      'VCO2 is always VCO1 translated by +308px. No independently tuned coordinate exists.',
      f'Source width is derived from max GDI Tahoma10px width(WAVE,ROM,USER)+2×11px inherited selector gutter, rounded up to the next even pixel: {source_width}px.',
      f'Actual font widths: {PRODUCTION["font"]["widths"]}. The table receives the remaining width. Table text uses the previous mockup 4px inset; source keeps the production symmetric11px arrow gutter.',
      '4px gaps and18px browser size are inherited from the existing table/browser row. No fonts or controls are shrunk.', '',
      '## Vertical construction and top-band fit', '',
      'Knobs, labels and readouts remain untouched: centersY93, face36px, labelY113 H13, readoutY127 H12. First compact selector begins142, leaving3px after the readout. SelectorH18;pitch20=18+2.',
      'Compact rows begin at142 and162; lower edge180. C moves the top-band bottom from202 to182, leaving2px from field lower edge to module boundary, the same clearance as the baseline third ROM row.',
      'All six modules pass actual-control extent checks. Bottommost controls in XMOD/MIXER/VCF/AMP are their unchanged selectors at162..180. Hard Sync ends at client126 including its label. Module headers and screw centers retain their native inset; side screws do not overlap content horizontally.',
      'C translates the complete216px middle band and150px matrix by−20px. Their internal offsets, controls and row pitches are unchanged; header/top-row controls do not move. Overall height=48+134+216+150=548. B retains568. The reference underlay ramp stays568px tall to preserve the header exactly; exposed inter-module gap pixels sample that same ramp at their new Y. All shifted module interiors remain pixel-identical.', '',
      '| Top module | Lowest control lower edge (client) | C boundary | Clearance |',
      '|---|---:|---:|---:|']
    for name in ('VCO1','XMOD','VCO2','MIXER','VCF','AMP'):
        lines.append(f'|{name}|180|182|2|')
    lines += ['', '## Exact variant rectangles', '',
      'Rectangles use X,Y,W,H. Shared GDI text centering and measured font ascent determine matching glyph baselines; ruler images include those baselines plus field edges, knob centers and panel boundaries. No label-specific offsets.', '']
    for variant in ('B','C'):
        v=g[variant]
        lines += [f'### {variant}', '',f'Size{v["size"]}; VCO{v["vco_dimensions"]}; content146; margin12; top band{v["top_band_height"]}; middle originY{v["middle_origin_client"]}; matrix originY{v["matrix_origin_client"]}.',
                  f'Knob centers{v["knob_centers"]};readouts{v["readouts"]}.', '',
                  '| Field | VCO1 | VCO2 |','|---|---|---|']
        for k,v1 in v['fields_vco1'].items():
            lines.append(f'|{k}|{v1}|{v["fields_vco2"][k]}|')
        lines += ['',f'Width: {v["width_equation"]}. Interpolation: {v["interpolation_equation"]}.',f'X rulers: {v["rulers_x"]}.',f'Y rulers: {v["rulers_y"]}.','']
    lines += ['### A, D and E', '',
      'A retains original source/engine68px fields atY142, table124px plus4px gap plus18px browser atY162, and68/10/68 interpolation fields atY182. Same knob/readout bounds as B/C.',
      'D uses C rectangles exactly: VCO1 ROM/PAD MAJ7/S:HOLD/F:MORPH; VCO2 USER/GLASS 07/S:LINEAR/F:STEP. GLASS 07 is a realistic hypothetical user-table display name (Glass 07.wav), not an imported file. Browser is still a static preview.',
      'E uses unchanged production analytic selectors: Wave/Engine captionsY147 H13; fieldsY162 H18, X12/90 and320/398 W68; VCO1 SAW UP/CLASSIC andVCO2 PULSE/CLEAN. Shape/PW remains visible. No dummy source/table/interpolation fields are painted. A redundant second row would duplicate source state; contextual empty space is shared and balanced instead.', '',
      '## Validation', '',*['- '+s for s in checks], '',
      'The build log concerns only an isolated screenshot renderer/probe, never a VST3. No production target was built. No interaction, reader, ROM integration, pulse fix, DSP, state, parameters, presets, themes, IDs, Git or publishing work was performed. Stop for visual review.']
    (OUT/'GEOMETRY-COMPARISON.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')


if __name__ == '__main__':
    OUT.mkdir(parents=True,exist_ok=True);WORK.mkdir(exist_ok=True)
    before=boundary()
    for p in (ROOT/'port').iterdir():
        if p.is_file():shutil.copyfile(p,WORK/p.name)
    for name in ('Editor.cpp','MockGeometry.h'):
        shutil.copyfile(BASE/name,WORK/name)
    original_editor=(WORK/'Editor.cpp').read_text()
    original_layout=(ROOT/'port/Layout.h').read_text()
    original_params=(ROOT/'port/ExpansionParameters.h').read_text()
    (WORK/'probe.cpp').write_text(PROBE)
    exe=compile_private(True)
    PRODUCTION=json.loads(subprocess.check_output([str(exe)],text=True))
    write_json('production-measured-geometry.json',PRODUCTION)
    assert PRODUCTION['size']==[960,568] and PRODUCTION['top_height']==154
    source_width = max(PRODUCTION['font']['widths'][s] for s in ('WAVE','ROM','USER'))+22
    source_width = (source_width+1)//2*2
    assert PRODUCTION['font']['widths']['PAD MAJ7'] <= 146-source_width-26-8
    assert max(PRODUCTION['font']['widths'][s] for s in ('S:LINEAR','F:MORPH'))<=68-8
    # Measure every top control's actual visible lower edge, including toggle label.
    for c in PRODUCTION['controls']:
        if c['control'][1]>=154:continue
        assert c['field'][1]+c['field'][3]<=132
        assert c['label_bounds'][1]+c['label_bounds'][3]<=132
    a=capture(BASE/'rom_mockup.exe','A-BASELINE')
    old=Image.open(ROOT/'artifacts/factory-rom-review/gui-mockups/main/rom.png').convert('RGB')
    assert equal_crop(a,old,(0,0,960,568))
    geometry={}
    images={}
    # Set realistic analytic values only in the private preview harness.
    preview=(WORK/'Preview.cpp').read_text().replace('#include <string>','#include <string>\n#include <cstdlib>')
    preview=preview.replace('  if (argc >= 3 &&', '  if(auto mode=std::getenv("XS_ROM_MOCK_MODE");mode&&std::atoi(mode)==0){state[24]=.4;state[25]=1;state[xs::Engine1]=0;state[xs::Engine2]=1;}\n  if (argc >= 3 &&')
    (WORK/'Preview.cpp').write_text(preview)
    for short,variant in ((False,'B'),(True,'C')):
        geometry[variant]=configure(short,original_editor,original_layout,original_params,source_width)
        exe=compile_private()
        images[variant]=capture(exe,variant+('-COMPACT-SHORTER' if short else '-COMPACT-VCO'))
        capture(exe,variant+'-RULERS',guides=True)
        shutil.copyfile(WORK/'StudyGeometry.h',OUT/(variant+'-geometry.h'))
        if short:
            images['D']=capture(exe,'D-MIXED-C-GEOMETRY',mode=2)
            images['E']=capture(exe,'E-ANALYTIC-C-GEOMETRY',mode=0)
    b,c=images['B'],images['C']
    assert equal_crop(a,b,(0,0,960,48))
    assert equal_crop(a,b,(170,48,308,202)) and equal_crop(a,b,(478,48,960,202))
    assert equal_crop(a,b,(0,202,960,568))
    assert equal_crop(b,c,(0,0,960,48))
    for name,x,y,w,h in PRODUCTION['panels'][6:]:
        box=(x+1,y+49,x+w-1,y+48+h-1)
        translated=(box[0],box[1]-20,box[2],box[3]-20)
        assert ImageChops.difference(b.crop(box),c.crop(translated)).getbbox() is None, name
    for variant,v in geometry.items():
        for name,rect in v['fields_vco1'].items():
            assert v['fields_vco2'][name]==shifted_rect(rect,308,0)
    assert boundary()==before
    write_json('PRODUCTION-BOUNDARY.json',dict(unchanged=True,file_count=len(before),sha256=before))
    write_json('geometry.json',geometry)
    checks=['Baseline re-render is pixel-identical to completed ROM mockup.',
      'B header, all non-VCO top modules and entire middle/matrix area are pixel-identical to baseline.',
      'C header is pixel-identical to B; each middle/matrix module interior is a pixel-identical −20px translation of B (exposed global-underlay gaps excluded).',
      'Both VCO field structures verified as exact +308px translations.',
      'Font extents, horizontal width equations and all six top-module content bounds checked.',
      f'All {len(before)} protected production/factory/docs files remain SHA-256 identical.',
      'Five full-panel clean previews plus B/C construction overlays generated in native STEEL.']
    report(geometry,source_width,checks)
    write_json('VALIDATION.json',dict(passed=True,checks=checks))
    print('\n'.join(checks).encode('ascii', 'replace').decode('ascii'))
