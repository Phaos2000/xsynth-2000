// GPL-2.0-or-later. Offline diagnostic; never allocates inside timed DSP loops.
#include "Core.h"
#include <array>
#include <chrono>
#include <cstdio>
#include <xmmintrin.h>
int main() {
  unsigned csr=_mm_getcsr();_mm_setcsr(csr|0x8040);
  struct Case {const char *name;int voices,clean,mode;bool matrix,automation;};
  const Case cases[]={{"legacy",0,0,0,false,false},{"raw-1",1,0,0,false,false},
    {"clean-1",1,2,0,false,false},{"hybrid-PM-8",8,1,2,true,false},
    {"raw-32",32,0,0,false,false},{"clean-32",32,2,0,false,false},
    {"clean-PM-matrix-32",32,2,2,true,false},{"matrix-gain-automation-8",8,1,1,true,true}};
  std::printf("case,median_ms_per_audio_second\n");
  for(auto &c:cases) {
    std::array<double,3> times{};
    for(auto &time:times) {
      xs::Engine e;e.reset(48000);e.set(24,.4);e.set(25,1);
      if(c.voices){e.set(xs::VoiceMode,1);int k=0;while(xs::VoiceCounts[k]!=c.voices)++k;e.set(xs::Voices,k/5.);}
      e.set(xs::Engine1,c.clean==2);e.set(xs::Engine2,c.clean>0);e.set(xs::XMode,c.mode/2.);e.set(xs::XDepth,c.mode?.2:0);
      if(c.matrix)for(int route=0;route<6;++route){int id=xs::MatrixFirst+route*3;const int source[]={1,2,3,4,9,10},destination[]={0,3,8,13,1,6};e.set(id,source[route]/10.);e.set(id+1,destination[route]/17.);e.set(id+2,.6);}
      for(int n=0;n<std::max(1,c.voices);++n)e.note(36+n,true,.8f);
      for(int i=0;i<4800;++i)e.next();
      auto start=std::chrono::steady_clock::now();double sum=0;
      for(int i=0;i<48000;++i){if(c.automation)e.set(23,.25+.5*(i%1024)/1023);sum+=e.next();}
      time=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
      volatile double sink=sum;(void)sink;
    }
    std::sort(times.begin(),times.end());std::printf("%s,%.4f\n",c.name,times[1]);
  }
  std::printf("DSP_instance_bytes,%zu\nShared_legacy_lookup_bytes,1212\n",sizeof(xs::Engine));
  _mm_setcsr(csr);return 0;
}
