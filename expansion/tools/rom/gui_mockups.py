"""Static design snapshots using a private COPY of the current GDI renderer.

No copied file is a production source or built by the plugin CMake targets.
"""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
from generate import ROOT,dump

OUT=ROOT/'artifacts/factory-rom-review'
WORK=ROOT/'artifacts/factory-rom/gui-mockup-source'
THEMES=['STEEL','98','INDUSTRIAL','MULTIMEDIA','2000','SAND']


def geometry():
    margin=12;selector=18;caption=14;gap=margin;half=(480-2*margin-gap)//2
    tab_y=8+caption+8;label_y=tab_y+selector+8;field_y=label_y+caption
    wave_y=field_y+selector+margin;wave_h=7*16
    pos_label=wave_y+wave_h+8;pos_field=pos_label+caption
    sample_label=pos_label+36;sample_field=sample_label+caption
    load=sample_field+selector+margin;user_label=load+selector+margin//2
    user_field=user_label+caption;status=user_field+selector+margin//2
    fields=[('VCO1',margin,tab_y,half,selector),('VCO2',margin+half+gap,tab_y,half,selector),
      ('BANK',margin,field_y,half,selector),('TABLE',margin+half+gap,field_y,half,selector),
      ('WAVEFORM',margin,wave_y,480-2*margin,wave_h),
      ('POSITION',margin,pos_field,half,selector),('FRAME READOUT',margin+half+gap,pos_field,half,selector),
      ('SAMPLE',margin,sample_field,half,selector),('FRAME MODE',margin+half+gap,sample_field,half,selector),
      ('LOAD SINGLE CYCLE...',margin,load,half,selector),('LOAD WAVETABLE...',margin+half+gap,load,half,selector),
      ('USER SLOT',margin,user_field,half,selector),('CLOSE',margin+half+gap,user_field,half,selector)]
    return dict(main_size=[960,568],browser_size=[480,384],header=48,origins=[0,308],vco_width=170,vco_height=154,
       margin=12,column_width=68,column_gap=10,selector_height=18,row_step=20,
       source_y_body=114-20,table_y_body=114,reader_y_body=114+20,
       table_width=146-selector-4,browser_button_width=selector,table_browser_gap=4,
       knob_centers_main=[[46,93],[124,93],[354,93],[432,93]],
       knob_face_size=36,knob_label_y_main=113,knob_label_height=13,
       knob_readout_y_main=127,knob_readout_size=[54,12],
       browser_fields=fields,browser_status_y=status,browser_label_height=13,
       browser_label_to_field=14,browser_margin=margin,browser_column_gap=gap,
       browser_rulers_x=[1,6,12,234,246,468,474,479],
       browser_rulers_y=sorted({8,22,tab_y,tab_y+18,label_y,field_y,field_y+18,wave_y,wave_y+wave_h,
                      pos_label,pos_field,pos_field+18,sample_label,sample_field,sample_field+18,
                      load,load+18,user_label,user_field,user_field+18,status,status+12}),
       main_rulers_x=[0,12,46,80,90,124,136,140,158,170,308,320,354,388,398,432,444,448,466,478,582,834,960],
       main_rulers_y=[48,52,66,75,93,111,113,126,127,139,142,160,162,180,182,200,202,232,247,267,281,293,319,337,355,373,391,409,418,568])


BROWSER_CODE=r'''
  void mockField(int x,int y,int w,int h,const char* value,bool button=false,bool dormant=false){
    if(button){gradient(x,y-48,w,h,{139,153,150},{52,82,88});bevel(x,y-48,w,h);}
    else well(x,y-48,w,h);
    text(x+4,y-48,w-8,h,value,dormant?RGB(120,145,148):RGB(127,215,216));
  }
  void mockExtraVcos(){
    const int mode=mockMode();
    for(int o=0;o<2;++o){int x=mockgrid::Origins[o]+mockgrid::Margin;
      const char* table=mode==0?"WAVE MODE":mode==2?(o?"USER CYCLE":"PAD AIR"):(o?"HORN 01":"PAD MAJ7");
      mockField(x,mockgrid::TableY+48,mockgrid::TableWidth,18,table,false,mode==0);
      mockField(x+mockgrid::TableWidth+mockgrid::TableBrowserGap,mockgrid::TableY+48,18,18,"...",true);
      mockField(x,mockgrid::ReaderY+48,68,18,mode==0?"S:--":o?"S:LINEAR":"S:HOLD",true,mode==0);
      mockField(x+78,mockgrid::ReaderY+48,68,18,mode==0?"F:--":"F:MORPH",true,mode==0);
    }
  }
  void mockRulers(bool browser){
    const COLORREF ink=RGB(218,118,76);
    const int w=browser?480:960,h=browser?384:568;
    if(browser){for(int x:mockgrid::BrowserRulersX)line(x,-48,x,h-48,ink);
      for(int y:mockgrid::BrowserRulersY)line(0,y-48,w,y-48,ink);}
    else {for(int x:mockgrid::MainRulersX)line(x,-48,x,h-48,ink);
      for(int y:mockgrid::MainRulersY)line(0,y-48,w,y-48,ink);
      for(auto c:mockgrid::KnobCenters){line(c[0]-5,c[1]-48,c[0]+6,c[1]-48,RGB(255,240,180));line(c[0],c[1]-53,c[0],c[1]-42,RGB(255,240,180));}}
  }
  void mockBrowserRender(){
    gradient(0,-48,480,384,{62,98,106},{25,56,62});
    gradient(2,-46,476,380,{49,83,91},{23,58,67});bevel(1,-47,478,382);
    for(int x:{6,474})for(int y:{6,378})screw(x,y-48);
    headerMaterial(12,8-48,456,14);text(12,8-48,456,14,"ROM PROGRAMMER",RGB(127,215,216),true);
    const char* values[]={"VCO 1","VCO 2","PAD","MAJ7","","46.7%","8 / 16","HOLD","MORPH", "LOAD SINGLE CYCLE...","LOAD WAVETABLE...","USER CYCLE","CLOSE"};
    const char* labels[]={"","","BANK","TABLE","","POSITION","FRAME","SAMPLE","FRAME","","","USER SLOT",""};
    for(int i=0;i<13;++i){auto b=mockgrid::BrowserFields[i];
      if(i==4){well(b.x,b.y-48,b.w,b.h);continue;}
      if(labels[i][0])text(b.x,b.y-48-14,b.w,13,labels[i],RGB(127,215,216));
      mockField(b.x,b.y,b.w,b.h,values[i],i<2||i==7||i==8||i==9||i==10||i==12,i==1);
    }
    auto b=mockgrid::BrowserFields[4];
    for(int n=1;n<8;++n)line(b.x+1+(b.w-2)*n/8.,b.y+1-48,b.x+1+(b.w-2)*n/8.,b.y+b.h-1-48,RGB(50,102,115));
    for(int n=1;n<4;++n)line(b.x+1,b.y+(b.h*n)/4.-48,b.x+b.w-1,b.y+(b.h*n)/4.-48,RGB(50,102,115));
    // Actual decoded PCM8 frame 7, drawn as ZOH steps on the GUI-only raster.
    int lastX=b.x+2;double lastY=b.y+b.h/2.-mockgrid::Wave[0]/32768.*(b.h/2-8);
    for(int n=1;n<=256;++n){int xx=b.x+2+(b.w-4)*n/256;double yy=b.y+b.h/2.-mockgrid::Wave[n%256]/32768.*(b.h/2-8);
      line(lastX,lastY-48,xx,lastY-48,RGB(127,215,216));line(xx,lastY-48,xx,yy-48,RGB(127,215,216));lastX=xx;lastY=yy;}
    text(12,mockgrid::BrowserStatusY-48,456,12,"FACTORY ROM | 8-BIT PCM",RGB(127,215,216));
    if(mockGuides())mockRulers(true);
  }
'''


def prepare():
    g=geometry();WORK.mkdir(parents=True,exist_ok=True)
    for p in (ROOT/'port').glob('*'):
        if p.is_file():shutil.copyfile(p,WORK/p.name)
    dump(OUT/'gui-mockups/geometry.json',g)
    data=json.loads((OUT/'data/decoded.json').read_text())
    header=['#pragma once','namespace mockgrid {',
      f'constexpr int Margin=12,SourceY={g["source_y_body"]},TableY={g["table_y_body"]},ReaderY={g["reader_y_body"]},TableWidth={g["table_width"]},TableBrowserGap=4,BrowserStatusY={g["browser_status_y"]};',
      'constexpr int Origins[]={0,308};',
      'constexpr int KnobCenters[][2]={{46,93},{124,93},{354,93},{432,93}};',
      'struct Rect {int x,y,w,h;};',
      'constexpr Rect BrowserFields[]={'+','.join('{'+','.join(map(str,b[1:]))+'}' for b in g['browser_fields'])+'};']
    for source,dest in (('browser_rulers_x','BrowserRulersX'),('browser_rulers_y','BrowserRulersY'),('main_rulers_x','MainRulersX'),('main_rulers_y','MainRulersY')):
        header.append('constexpr int '+dest+'[]={'+','.join(map(str,g[source]))+'};')
    header+=['constexpr int Wave[]={'+','.join(map(str,data[32]['frames'][7]))+'};','}']
    (WORK/'MockGeometry.h').write_text('\n'.join(header)+'\n',encoding='utf-8')
    editor=(WORK/'Editor.cpp').read_text()
    editor=editor.replace('#include "Editor.h"','#include "Editor.h"\n#include "MockGeometry.h"\n#include <cstdlib>')
    editor=editor.replace('HINSTANCE module() {', '''int mockMode(){auto p=std::getenv("XS_ROM_MOCK_MODE");return p?std::atoi(p):0;}
bool mockBrowser(){return std::getenv("XS_ROM_MOCK_BROWSER")!=nullptr;}
bool mockGuides(){return std::getenv("XS_ROM_MOCK_GUIDES")!=nullptr;}
HINSTANCE module() {''')
    editor=editor.replace('scale = width / double(NativeWidth);','scale = width / double(mockBrowser()?480:NativeWidth);')
    editor=editor.replace('    double n = s[id];','    if(mockMode() && (id==1||id==3))label="Position";\n    double n = s[id];')
    editor=editor.replace('void selector(const Control &c,const State &s) {','''void selector(const Control &original,const State &s) {
    Control c=original;bool mockVco=c.id==24||c.id==25||c.id==Engine1||c.id==Engine2;
    if(mockVco){c.y=mockgrid::SourceY;c.label="";}''')
    editor=editor.replace('char value[96]{}; valueText(effectiveId(c,s),s,value,sizeof value);','''char value[96]{}; valueText(effectiveId(c,s),s,value,sizeof value);
    if(mockVco && mockMode() && (c.id==24||c.id==25))std::snprintf(value,sizeof value,"%s",mockMode()==2&&c.id==25?"USER":"ROM");''')
    editor=editor.replace('bool dropdown=isDropdown(c,s),pressed=pressedParam==c.id;',
        'bool dropdown=isDropdown(c,s),pressed=pressedParam==c.id; if(mockVco&&(c.id==24||c.id==25))dropdown=true;')
    editor=editor.replace('moduleGrid::SelectorHeight-4,value,RGB(127,215,216));','moduleGrid::SelectorHeight-4,value,mockVco&&mockMode()&&(c.id==Engine1||c.id==Engine2)?RGB(120,145,148):RGB(127,215,216));')
    editor=editor.replace('  void render(const State &s) {',BROWSER_CODE+'\n  void render(const State &s) {\n    if(mockBrowser()){if(!dc)prepare();mockBrowserRender();return;}')
    editor=editor.replace('    for(int tab=0;tab<4;++tab)', '    mockExtraVcos();\n    for(int tab=0;tab<4;++tab)')
    editor=editor.replace('    if (aboutOpen)\n      aboutPanel();','    if (aboutOpen)\n      aboutPanel();\n    if(mockGuides())mockRulers(false);')
    editor=editor.replace('resize(NativeWidth * percent / 100, NativeHeight * percent / 100);','resize((mockBrowser()?480:NativeWidth)*percent/100,(mockBrowser()?384:NativeHeight)*percent/100);')
    (WORK/'Editor.cpp').write_text(editor,encoding='utf-8')
    vs=r'C:\Program Files\Microsoft Visual Studio\18\Community'
    cmd=f'''@echo off
call "{vs}\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /O2 /MD /DNOMINMAX /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /I"{WORK}" "{WORK/'Editor.cpp'}" "{WORK/'Preview.cpp'}" /Fo"{WORK}/" /Fe"{WORK/'rom_mockup.exe'}" /link /SUBSYSTEM:WINDOWS gdiplus.lib comdlg32.lib ole32.lib shell32.lib gdi32.lib user32.lib advapi32.lib
exit /b %errorlevel%
'''
    (WORK/'build.cmd').write_text(cmd,encoding='utf-8')
    print(WORK/'build.cmd')


def render():
    dump(OUT/'gui-mockups/geometry.json',geometry())
    exe=WORK/'rom_mockup.exe'
    for mode,name in ((0,'normal'),(1,'rom'),(2,'mixed')):
        for guides in (False,True):
            env=dict(os.environ,XS_ROM_MOCK_MODE=str(mode));env.pop('XS_ROM_MOCK_BROWSER',None);env.pop('XS_ROM_MOCK_GUIDES',None)
            if guides:env['XS_ROM_MOCK_GUIDES']='1'
            path=OUT/'gui-mockups'/('alignment-guides' if guides else 'main')/(name+'.png');path.parent.mkdir(parents=True,exist_ok=True)
            subprocess.run([str(exe),'--snapshot',str(path),'100','--init','--theme','STEEL'],env=env,check=True)
    for theme in THEMES:
        for guides in (False,True):
            env=dict(os.environ,XS_ROM_MOCK_BROWSER='1');env.pop('XS_ROM_MOCK_GUIDES',None)
            if guides:env['XS_ROM_MOCK_GUIDES']='1'
            path=OUT/'gui-mockups'/('alignment-guides' if guides else 'rom-programmer')/('browser-'+theme+'.png');path.parent.mkdir(parents=True,exist_ok=True)
            subprocess.run([str(exe),'--snapshot',str(path),'100','--theme',theme],env=env,check=True)
    geometry_report()
    print('18 native mockups produced; all six browser themes use identical geometry')


def geometry_report():
    g=geometry();rows=[]
    for origin in g['origins']:
        for label,column in (('Wave / Source',0),('Engine',1)):
            rows.append([f'VCO at {origin}: {label}',origin+12+78*column,142,68,18])
        rows += [[f'VCO at {origin}: Table',origin+12,162,124,18], [f'VCO at {origin}: Browser',origin+140,162,18,18],
                 [f'VCO at {origin}: Sample',origin+12,182,68,18],[f'VCO at {origin}: Frame',origin+90,182,68,18]]
    lines=['# GUI mockup geometry — DESIGN ONLY','',
       'Production Editor/Layout and parameter/state code remain unchanged. A private snapshot COPY',
       'reuses current GDI materials, knobs, typography, title and palettes; it is never built by VST CMake.',
       'The copy draws static visual placeholders only: it does not bind old Shape IDs to Position.',
       'Future Position must use distinct new IDs described in the architecture. No new GUI behavior is implemented.','',
       '## Shared geometry','',
       'Inherited: native 960×568, header 48, VCO origins 0/308, widths 170, top-row height154,',
       'margins12, columns68 with gap10, 36px knobs, 54×12 readouts, 18px selectors, 10px Tahoma.',
       'New proposed strip: row step20=18+2; row Y (body)114−20,114,114+20; client adds48.',
       'Table width124=146−18−4; browser square18; table/browser gap4. Bottom edge200 leaves',
       'two pixels before module end202. Readout ends139; first field begins142: three-pixel clearance.',
       'That deliberate tight clearance is shown for approval, not hidden by scaling or smaller fonts.',
       'External Wave/Engine captions are removed in this compact proposal; source values and S:/F:',
       'prefixes plus future menu descriptions identify roles. This readability tradeoff needs approval.',
       'Knob centers: (46,93),(124,93),(354,93),(432,93). Labels at Y113,H13; readouts Y127,H12.',
       'Pitch readouts X19/327; contextual readouts X97/405, all W54. Label text uses current',
       'GDI vertical centering in these exact rectangles. Matching label glyph baselines are identical;',
       'they derive from the same Tahoma ascent in the same-height rectangles, never per-label offsets.','',
       '| Field | X | Y | W | H |','|---|---:|---:|---:|---:|']
    lines += ['| '+' | '.join(map(str,row))+' |' for row in rows]
    lines+=['','## Browser, same layout in six themes','',
       '480×384 client; width=2 current 240px middle modules; height=32×12. Margin12, gutter12,',
       'two columns222=(480−24−12)/2, fields/buttons H18, caption-to-field14 (13px label+1).',
       'All corresponding rectangles are shared. Position and sample groups are36px apart.',
       'Waveform H112=7×16 raster rows; shell inset1; screws at margin/2 from corners.',
       'Section gaps are inherited12, half-margin6, existing header gap8 or documented field gap4.',
       'Full equations and ruler coordinates are machine-readable in geometry.json and the single',
       'generated MockGeometry.h consumed by every snapshot. No theme has coordinate offsets.','',
       '| Browser field | X | Y | W | H |','|---|---:|---:|---:|---:|']
    lines+=['| '+' | '.join(map(str,row))+' |' for row in g['browser_fields']]
    lines+=['',f"Main vertical rulers: {g['main_rulers_x']}",f"Main horizontal rulers: {g['main_rulers_y']}",
            f"Browser vertical rulers: {g['browser_rulers_x']}",f"Browser horizontal rulers: {g['browser_rulers_y']}",'',
       'Clean images are in main/ and rom-programmer/. Ruler versions are in alignment-guides/.',
       'Browser waveform is actual decoded PCM8 PAD MAJ7 frame7, displayed with crisp HOLD steps.',
       'USER text is a design placeholder, not an imported/implemented user table. BAND text denotes',
       'a future selectable reader; no BAND DSP is implemented. Window chrome, import dialogs and',
       'interaction rules remain design-only. GUI architecture is not approved by these snapshots.']
    (OUT/'gui-mockups/GEOMETRY.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')


if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--prepare',action='store_true');ap.add_argument('--render',action='store_true')
    ap.add_argument('--output',type=Path,default=OUT)
    a=ap.parse_args();OUT=a.output.resolve()
    if a.prepare:prepare()
    if a.render:render()
