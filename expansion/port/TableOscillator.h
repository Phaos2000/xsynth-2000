// GPL-2.0-or-later. Two bounded heads with four existing topology phase histories.
#pragma once
#include "TableReader.h"
#include "DigitalOscillator.h"
namespace xs { namespace table {
struct Selection {
  const Table* data=nullptr;
  int source=0,wave=0,renderer=0,owner=-1;
  float gain=1;
  Sample sample=Sample::Hold; Frame frame=Frame::Step;
  int root()const{return source&&data?data->rootBin:1;}
  double positionFrequency(double hz,float position)const{return source==1&&data?positionTuning::frequency(data->id,hz,position):hz;}
  bool operator==(const Selection&b)const{return data==b.data&&source==b.source&&wave==b.wave&&renderer==b.renderer&&owner==b.owner&&gain==b.gain&&sample==b.sample&&frame==b.frame;}
  bool operator!=(const Selection&b)const{return !(*this==b);}
};
struct Oscillator {
  struct Head {
    Selection selection;
    std::array<double,4> phase{};
    BandChoice band{};FrameChoice frames{};
    double lastHz=-1,lastSr=-1,lastRootHz=-1,lastClockSr=-1,cycleHz=0,increment=0;float lastPosition=-1;
    std::array<float,4> previousPulseDuty{{-1,-1,-1,-1}};
    void assign(Selection c){selection=c;previousPulseDuty.fill(-1);lastHz=lastSr=lastRootHz=lastClockSr=-1;lastPosition=-1;}
    template<class Analytic> float read(int path,double hz,float pm,float position,float shape,double sr,bool&wrapped,Analytic analytic,bool silent=false){
      hz=selection.positionFrequency(hz,position);
      if(hz!=lastRootHz||sr!=lastClockSr){cycleHz=hz/selection.root();increment=cycleHz/sr;lastRootHz=hz;lastClockSr=sr;}
      double frequency=cycleHz,advanced=phase[path]+increment;
      wrapped=advanced>=1||advanced<0;
      phase[path]=advanced>=0&&advanced<1?advanced:wrap(advanced);
      if(silent)return 0;
      double addressed=phase[path]+pm;addressed=addressed>=0&&addressed<1?addressed:wrap(addressed);
      if(!selection.source){float sample=analytic(float(addressed),float(frequency/sr),selection.wave,shape,selection.renderer);
        if(selection.wave==5&&selection.renderer==1){float duty=oscillator::pulseDuty(shape);sample=oscillator::movingPulse(sample,float(addressed),float(frequency/sr),duty,previousPulseDuty[path]);previousPulseDuty[path]=duty;}else previousPulseDuty[path]=-1;
        return sample;
      }
      if(!selection.data)return 0;
      const auto&t=*selection.data;
      if(position!=lastPosition){frames=chooseFrames(t,position,selection.frame);lastPosition=position;}
      if(selection.sample==Sample::Band&&(frequency!=lastHz||sr!=lastSr)){
        band=chooseBand(t,frequency,sr);lastHz=frequency;lastSr=sr;
      }
      // Common HOLD/LINEAR reads do not need a BAND ReadPlan or unused
      // interpolation addresses. Arithmetic order matches plannedFrame/planned.
      if(selection.sample!=Sample::Band){
        double x=addressed*t.length;int a=std::min(t.length-1,int(x));
        if(selection.sample==Sample::Hold){float first=t.pcm[size_t(frames.a)*t.length+a];
          if(frames.blend!=0&&frames.a!=frames.b)first=first+frames.blend*(t.pcm[size_t(frames.b)*t.length+a]-first);
          return first*selection.gain;
        }
        int b=a+1==t.length?0:a+1;float fraction=float(x-a);
        auto sample=[&](int frame){const float*p=t.pcm.data()+size_t(frame)*t.length;float first=p[a];return first+fraction*(p[b]-first);};
        float first=sample(frames.a);if(frames.blend!=0&&frames.a!=frames.b)first=first+frames.blend*(sample(frames.b)-first);
        return first*selection.gain;
      }
      return planned(t,frames,plan(t,addressed,selection.sample,band))*selection.gain;
    }
  } current,target;
  Selection pending;
  bool ready=false,active=false,hasPending=false;
  int progress=0,length=2;
  void reset(Selection c,const std::array<std::array<float,2>,4>&phases,int o,double sr){
    current.assign(c);for(int t=0;t<4;++t)current.phase[t]=phases[t][o];
    target=current;ready=true;active=hasPending=false;progress=0;length=std::max(2,int(sr*.002));
  }
  void start(Selection c){target.assign(c);for(int t=0;t<4;++t)target.phase[t]=transfer(current.phase[t],current.selection.root(),c.root(),PhaseTransfer::RootNearest);active=true;progress=0;}
  void request(Selection c){if(active){pending=c;hasPending=c!=target.selection;}else if(c!=current.selection)start(c);}
  void sync(int path){current.phase[path]=target.phase[path]=0;}
  template<class Analytic> float read(int path,double hz,float pm,float position,float shape,double sr,bool&wrapped,Analytic analytic,bool silent=false){
    bool oldWrapped=false,newWrapped=false;float weight=active?float(progress)/(length-1):0;
    if(!silent&&active&&current.selection.data&&target.selection.data&&current.selection.frame==target.selection.frame&&current.selection.data->frames==target.selection.data->frames){
      if(current.lastPosition!=position){current.frames=chooseFrames(*current.selection.data,position,current.selection.frame);current.lastPosition=position;}
      target.frames=current.frames;target.lastPosition=position;
    }
    float a=current.read(path,hz,pm,position,shape,sr,oldWrapped,analytic,silent||(active&&weight==1));
    if(!active){wrapped=oldWrapped;return a;}
    float b=target.read(path,hz,pm,position,shape,sr,newWrapped,analytic,silent||weight==0);
    wrapped=newWrapped;
    return weight==0?a:weight==1?b:a+weight*(b-a);
  }
  void advance(){if(active&&++progress>=length){current=target;active=false;if(hasPending){auto c=pending;hasPending=false;if(c!=current.selection)start(c);}}}
  float phase(int path)const{return float((active?target:current).phase[path]);}
  unsigned owners()const{
    unsigned mask=0;
    auto add=[&](const Selection&s){if(s.owner>=0)mask|=1u<<s.owner;};
    if(ready){add(current.selection);if(active)add(target.selection);if(hasPending)add(pending);}
    return mask;
  }
};
}} // namespace xs::table
