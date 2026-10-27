// GPL-2.0-or-later. Bounded RIFF/WAVE decoder; call only off the render thread.
#pragma once
#include "Core.h"
namespace xs {namespace table {
struct WaveData {int channels=0,bits=0,validBits=0,format=0;uint32_t rate=0;std::vector<float>mono;float peak=0;bool cancellation=false;int suggestedLength=0;};
inline WaveData decodeWave(const std::vector<uint8_t>&bytes,int channel=2){
  using documentState::u32;
  if(bytes.size()<12||bytes.size()>32*1024*1024||std::memcmp(bytes.data(),"RIFF",4)||std::memcmp(bytes.data()+8,"WAVE",4)||uint64_t(u32(bytes.data()+4))+8!=bytes.size())throw std::runtime_error("Expected bounded little-endian RIFF/WAVE");
  auto u16=[](const uint8_t*p){return unsigned(p[0])|(unsigned(p[1])<<8);};
  const uint8_t*fmt=nullptr,*data=nullptr;size_t fmtLen=0,dataLen=0;int suggestion=0;
  for(size_t at=12;at<bytes.size();){
    if(bytes.size()-at<8)throw std::runtime_error("Truncated RIFF chunk");const auto*p=bytes.data()+at;size_t length=u32(p+4);at+=8;
    if(length>bytes.size()-at)throw std::runtime_error("Invalid RIFF chunk length");
    if(!std::memcmp(p,"fmt ",4)){if(fmt)throw std::runtime_error("Duplicate fmt");fmt=p+8;fmtLen=length;}
    if(!std::memcmp(p,"data",4)){if(data)throw std::runtime_error("Duplicate data");data=p+8;dataLen=length;}
    // Length is an informational suggestion only; explicit segmentation wins.
    if(!std::memcmp(p,"clm ",4)){
      if(length<4||std::memcmp(p+8,"<!>",3))throw std::runtime_error("Malformed clm metadata");
      std::string text(reinterpret_cast<const char*>(p+11),length-3);char*end=nullptr;long n=std::strtol(text.c_str(),&end,10);
      if(end==text.c_str()||n<16||n>4096||(suggestion&&suggestion!=n))throw std::runtime_error("Invalid/conflicting wavetable metadata");suggestion=int(n);
    }
    if(!std::memcmp(p,"srge",4)){
      if(length!=8||u32(p+8)!=1)throw std::runtime_error("Unsupported srge metadata");int n=int(u32(p+12));
      if(n<16||n>4096||(suggestion&&suggestion!=n))throw std::runtime_error("Invalid/conflicting wavetable metadata");suggestion=n;
    }
    at+=length;if(length&1){if(at>=bytes.size())throw std::runtime_error("Missing RIFF padding");++at;}
  }
  if(!fmt||fmtLen<16||!data)throw std::runtime_error("Missing fmt/data");
  WaveData result;result.format=u16(fmt);result.channels=u16(fmt+2);result.rate=u32(fmt+4);int align=u16(fmt+12);result.bits=u16(fmt+14);result.validBits=result.bits;result.suggestedLength=suggestion;
  if(result.format==0xfffe){
    static constexpr uint8_t suffix[]={0,0,0x10,0,0x80,0,0,0xaa,0,0x38,0x9b,0x71};
    if(fmtLen<40||u16(fmt+16)<22||u16(fmt+16)>fmtLen-18||std::memcmp(fmt+28,suffix,12))throw std::runtime_error("Unsupported extensible GUID");
    result.validBits=u16(fmt+18);result.format=int(u32(fmt+24));
  }else if(fmtLen!=16&&(fmtLen<18||u16(fmt+16)>fmtLen-18))throw std::runtime_error("Invalid fmt extension");
  bool pcm=result.format==1, floating=result.format==3;
  if((!pcm&&!floating)||result.channels<1||result.channels>2||!result.rate||channel<0||channel>2||result.validBits<1||result.validBits>result.bits||
      (pcm&&result.bits!=8&&result.bits!=16&&result.bits!=24&&result.bits!=32)||(floating&&result.bits!=32&&result.bits!=64)||(floating&&result.validBits!=result.bits))throw std::runtime_error("Unsupported WAV sample format");
  int width=result.bits/8;
  if(align!=width*result.channels||u32(fmt+8)!=uint64_t(result.rate)*align||dataLen%align||dataLen==0||dataLen/align>1048576)throw std::runtime_error("Invalid block alignment or sample count");
  result.mono.resize(dataLen/align);double inputPower=0,outputPower=0;
  auto sample=[&](const uint8_t*p){double v;
    if(floating){if(width==4){float f;std::memcpy(&f,p,4);v=f;}else std::memcpy(&v,p,8);}
    else if(width==1){int unused=8-result.validBits;if(unused&&(p[0]&((1u<<unused)-1)))throw std::runtime_error("Nonzero unused valid-bit padding");v=(int(p[0])-128)/128.;}
    else{uint32_t bits=0;for(int i=0;i<width;++i)bits|=uint32_t(p[i])<<(8*i);int32_t signedValue=int32_t(bits<<(32-result.bits));signedValue>>=32-result.bits;
      int unused=result.bits-result.validBits;
      if(unused&&(bits&((uint32_t(1)<<unused)-1)))throw std::runtime_error("Nonzero unused valid-bit padding");
      v=double(signedValue)/std::ldexp(1.,result.bits-1);}
    if(!std::isfinite(v)||std::abs(v)>8)throw std::runtime_error("Invalid/nonfinite WAV sample");return v;
  };
  for(size_t i=0;i<result.mono.size();++i){double left=sample(data+i*align),right=result.channels==2?sample(data+i*align+width):left;
    double x=channel==0?left:channel==1?right:(left+right)*.5;result.mono[i]=float(x);result.peak=std::max(result.peak,float(std::abs(x)));inputPower+=left*left+right*right;outputPower+=x*x*2;}
  result.cancellation=result.channels==2&&inputPower>1e-20&&outputPower<inputPower*.01;
  return result;
}
inline std::shared_ptr<const UserTable> importWave(WaveData wave,int length,std::string name,bool removeDC=false,bool normalize=false){
  if(length<16||length>4096||wave.mono.size()%length||wave.mono.size()/length>256)throw std::runtime_error("Choose an exact16..4096 sample cycle length,1..256 frames");
  int frames=int(wave.mono.size()/length);
  if(removeDC)for(int f=0;f<frames;++f){double sum=0;for(int i=0;i<length;++i)sum+=wave.mono[size_t(f)*length+i];float mean=float(sum/length);for(int i=0;i<length;++i)wave.mono[size_t(f)*length+i]-=mean;}
  float peak=0;for(float x:wave.mono)peak=std::max(peak,std::abs(x));
  if(peak>1&&!normalize)throw std::runtime_error("Peak exceeds1: choose global normalization or cancel");
  if(normalize&&peak>0)for(float&x:wave.mono)x/=peak;
  return prepareUser(std::move(wave.mono),length,frames,std::move(name),wave.rate,(removeDC?1:0)|(normalize?2:0));
}
}}
