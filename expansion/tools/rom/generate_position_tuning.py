"""Generate/check original equal-octave Position ratios; no external tuning data."""
from pathlib import Path
import argparse
def source():
 lines=['// GPL-2.0-or-later. Original equal-division scale clocks; no imported tuning data.','#pragma once','#include <algorithm>','#include <cmath>','namespace xs::table::positionTuning {','constexpr int divisions(int id){return id==66?19:id==67?24:id==68?31:0;}']
 for n in [19,24,31]:lines.append(f'inline constexpr double Ratios{n}[]={{'+','.join(repr(2.**(k/n)) for k in range(n+1))+'};')
 lines+=['inline int degree(int edo,float position){return std::clamp(int(std::floor(double(std::clamp(position,0.f,1.f))*edo+.5)),0,edo);}',
 'inline double ratio(int id,float position){int n=divisions(id);if(!n)return 1;int k=degree(n,position);return n==19?Ratios19[k]:n==24?Ratios24[k]:Ratios31[k];}',
 'inline double frequency(int id,double hz,float position){return divisions(id)?hz*ratio(id,position):hz;}','}']
 return '\n'.join(lines)+'\n'
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--check',action='store_true');a=p.parse_args();target=Path(__file__).resolve().parents[2]/'port/PositionTuning.h'
 if a.check:assert target.read_text()==source(),'Position tuning header differs'
 else:target.write_text(source())
 print('Verified original 19/24/31 EDO ratio header.' if a.check else 'Generated Position tuning header.')
