// Permanent offline production-reader integrity probe + controlled MAJ7 evidence.
#include "Core.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
namespace t=xs::table;
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
double hz(int n){return 440*std::exp2((n-69)/12.);}
struct Meter{double sum=0,sq=0,peak=0,jump=0,last=0;size_t n=0;
 void add(float x){check(std::isfinite(x),"nonfinite reader");sum+=x;sq+=double(x)*x;peak=std::max(peak,double(std::abs(x)));if(n)jump=std::max(jump,std::abs(x-last));last=x;++n;}};
void state(xs::Engine&e,xs::State&s){for(int i=0;i<xs::Count;++i)e.set(i,s[i]);}
int main(int argc,char**argv){try{
 check(argc==3,"experimental reference bank and output required");auto out=std::filesystem::path(argv[2]);std::filesystem::create_directories(out);
 const auto&bank=t::factoryBank();t::Bank references(argv[1]);std::ofstream csv(out/"reader-integrity.csv");
 csv<<"id,frame,reader,frame_mode,note,sample_rate,scenario,peak,max_sample_step\n"<<std::setprecision(12);
 size_t assertions=0,rows=0;auto expect=[&](bool b,const char*m){++assertions;check(b,m);};
 for(const auto&a:bank.tables)for(double sr:{44100.,48000.,96000.})for(int sample=0;sample<3;++sample)for(int fm=0;fm<2;++fm){
  for(int frame=0;frame<a.frames;++frame)for(int note:{24,48,84}){
   Meter m;double p=1.-1e-7,f=hz(note)/a.rootBin;
   for(int n=0;n<512;++n){p=t::wrap(p+f/sr);float x=t::read(a,p,float(frame)/(a.frames-1),t::Sample(sample),t::Frame(fm),f,sr);m.add(x);}
   csv<<a.id<<','<<frame<<','<<sample<<','<<fm<<','<<note<<','<<sr<<",STATIC,"<<m.peak<<','<<m.jump<<'\n';++rows;
   for(double phase:{0.,.125,.75,1.-1./1048576}){
    float x=t::read(a,phase,float(frame)/(a.frames-1),t::Sample(sample),t::Frame(fm),f,sr);
    float y=t::read(a,phase+1,float(frame)/(a.frames-1),t::Sample(sample),t::Frame(fm),f,sr);
    expect(x==y,"periodic address equivalence");
   }
  }
  for(int note:{24,48,84}){
   Meter m;double p=.991,f=hz(note)/a.rootBin;
   for(int n=0;n<8192;++n){p=t::wrap(p+f/sr);m.add(t::read(a,p,float(n)/8191,t::Sample(sample),t::Frame(fm),f,sr));}
   csv<<a.id<<",-1,"<<sample<<','<<fm<<','<<note<<','<<sr<<",SWEEP,"<<m.peak<<','<<m.jump<<'\n';++rows;
  }
  for(int frame=0;frame<a.frames;++frame)for(int i=0;i<a.length;++i){
   float x=t::frameRead(a,frame,(i+.25)/a.length,t::Sample::Hold,100,sr);
   expect(x==a.pcm[size_t(frame)*a.length+i],"HOLD exact code");
   float one=a.pcm[size_t(frame)*a.length+i],two=a.pcm[size_t(frame)*a.length+(i+1)%a.length];
   expect(std::abs(t::frameRead(a,frame,(i+.25)/a.length,t::Sample::Linear,100,sr)-(one+.25f*(two-one)))<1e-7,"cyclic LINEAR including boundary");
  }
  if(fm==1)for(int frame=1;frame<a.frames-1;++frame){float p=float(frame)/(a.frames-1);float one=t::read(a,.173,p-1e-7f,t::Sample(sample),t::Frame::Morph,300./a.rootBin,sr),two=t::read(a,.173,p+1e-7f,t::Sample(sample),t::Frame::Morph,300./a.rootBin,sr);expect(std::abs(one-two)<1e-5,"MORPH boundary continuity");}
 }
 // A relocated storage seam with compensated phase MUST preserve the signal.
 auto rotated=bank.tables[32];int shift=rotated.length/4;
 for(int f=0;f<rotated.frames;++f)std::rotate(rotated.pcm.begin()+f*rotated.length,rotated.pcm.begin()+f*rotated.length+shift,rotated.pcm.begin()+(f+1)*rotated.length);
 for(auto&l:rotated.levels)if(l.length>1)for(int f=0;f<rotated.frames;++f)std::rotate(l.samples.begin()+f*l.length,l.samples.begin()+f*l.length+l.length/4,l.samples.begin()+(f+1)*l.length);
 float rotationError=0;
 for(int s=0;s<3;++s)for(int fm=0;fm<2;++fm)for(float pos:{0.f,.5f,1.f})for(int i=0;i<1024;++i){double p=(i+.25)/1024;
  float a=t::read(bank.tables[32],p,pos,t::Sample(s),t::Frame(fm),hz(24)/8,48000),b=t::read(rotated,t::wrap(p-.25),pos,t::Sample(s),t::Frame(fm),hz(24)/8,48000);rotationError=std::max(rotationError,std::abs(a-b));expect(a==b,"seam relocation changes signal");}
 std::ofstream raw(out/"diagnostic.f32",std::ios::binary),idx(out/"diagnostic.csv");size_t offset=0;
 idx<<"variant,reader,frame,note,actual_cycle_hz,target_cycle_hz,sample_rate,cycles,offset,count\n"<<std::setprecision(15);
 for(int variant=0;variant<4;++variant)for(int s=0;s<(variant?1:3);++s)for(int frame:{0,7,15})for(int note:{24,48,84}){
  const auto&a=variant?references.tables[variant-1]:bank.tables[32];double target=hz(note)/8;int count=int(std::round(8*48000/target));double f=8.*48000/count,p=0;std::vector<float>v(count);
  for(auto&x:v){p=t::wrap(p+8./count);x=2*t::frameRead(a,frame,p,variant?t::Sample::Linear:t::Sample(s),f,48000);}
  idx<<variant<<','<<(variant?1:s)<<','<<frame<<','<<note<<','<<f<<','<<target<<",48000,8,"<<offset<<','<<count<<'\n';raw.write(reinterpret_cast<char*>(v.data()),v.size()*4);offset+=v.size();
 }
 std::ofstream audio(out/"listening.f32",std::ios::binary),index(out/"listening.csv");offset=0;index<<"label,offset,count\n";
 auto emit=[&](std::string label,std::vector<float>&v){for(float x:v)check(std::isfinite(x),"nonfinite listening");index<<label<<','<<offset<<','<<v.size()<<'\n';audio.write(reinterpret_cast<char*>(v.data()),v.size()*4);offset+=v.size();};
 auto render=[&](const t::Table&a,int sample,int fm,int note,bool moving,std::string label){std::vector<float>v(192000);double p=0,f=hz(note)/a.rootBin;
  for(size_t n=0;n<v.size();++n){p=t::wrap(p+f/48000);float pos=moving?float(n)/(v.size()-1):1;v[n]=2*t::read(a,p,pos,t::Sample(sample),t::Frame(fm),f,48000);}emit(label,v);};
 const char*names[]={"HOLD","LINEAR","BAND"};
 for(int s=0;s<3;++s){render(bank.tables[32],s,1,24,false,std::string("MAJ7_C1_STATIC_")+names[s]);render(bank.tables[32],s,1,24,true,std::string("MAJ7_C1_MOTION_MORPH_")+names[s]);}
 render(bank.tables[32],0,0,24,true,"MAJ7_C1_MOTION_STEP_HOLD");
 for(int s=0;s<2;++s)render(bank.tables[32],s,1,48,false,std::string("MAJ7_C3_STATIC_")+names[s]);
 for(int i=1;i<3;++i)render(references.tables[i],1,1,24,false,i==1?"CONTROL_MAJ7_C1_UNQUANTIZED_CONTINUOUS":"CONTROL_MAJ7_C1_PHASE_DISPERSED");
 for(int s=0;s<2;++s){auto e=std::make_unique<xs::Engine>();auto state=xs::defaults();state[xs::VoiceMode]=1;state[7]=0;state[23]=.5;state[21]=1;state[22]=0;state[5]=state[6]=state[13]=state[14]=state[19]=state[20]=0;state[xs::Keytrack]=.5;state[xs::oscParam(0,0)]=.5;state[xs::oscParam(0,1)]=32/65535.;state[xs::oscParam(0,3)]=1;state[xs::oscParam(0,4)]=s/2.;state[xs::oscParam(0,5)]=1;
  e->setAssets(&bank);::state(*e,state);e->reset(48000);e->next();e->note(24,true,1);std::vector<float>v(192000);for(auto&x:v)x=e->next();emit(std::string("ENGINE_MAJ7_C1_")+names[s],v);}
 for(int s=1;s<3;++s)render(bank.tables[25],s,1,84,true,std::string("GLASS_C6_MOTION_")+names[s]);
 for(auto item:{std::pair<int,const char*>{18,"AIR"},{23,"EPIC"},{26,"CHOIR"},{28,"MOTION"}})render(bank.tables[item.first],2,1,48,true,std::string(item.second)+"_C3_BAND_MOTION");
 std::ofstream status(out/"STATUS.json");status<<"{\"tables\":"<<bank.tables.size()<<",\"reader_rows\":"<<rows<<",\"assertions\":"<<assertions<<",\"seam_relocation_error\":"<<rotationError<<",\"all_finite\":true}\n";
 std::cout<<"PASS "<<assertions<<" assertions; "<<rows<<" current-bank reader cases; seam relocation error="<<rotationError<<'\n';
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
