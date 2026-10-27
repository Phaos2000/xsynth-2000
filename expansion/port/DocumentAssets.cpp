// GPL-2.0-or-later. All routines in this file run outside process().
#include "Core.h"
#include <complex>
#include <mutex>
#include <sstream>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
namespace xs { namespace documentState {
std::array<uint8_t,32> contentHash(int n,int frames,const std::vector<float>&samples){
  BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
  if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("SHA256 unavailable");
  ULONG bytes=0,got=0;BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&bytes),sizeof bytes,&got,0);
  std::vector<uint8_t>object(bytes);std::array<uint8_t,32>result{};
  NTSTATUS status=BCryptCreateHash(algorithm,&hash,object.data(),bytes,nullptr,0,0);
  uint8_t dimensions[8];for(int i=0;i<4;++i){dimensions[i]=uint8_t(uint32_t(n)>>(8*i));dimensions[4+i]=uint8_t(uint32_t(frames)>>(8*i));}
  if(status>=0)status=BCryptHashData(hash,dimensions,8,0);
  if(status>=0)status=BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<float*>(samples.data())),ULONG(samples.size()*4),0);
  if(status>=0)status=BCryptFinishHash(hash,result.data(),32,0);
  if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);
  if(status<0)throw std::runtime_error("SHA256 failed");return result;
}
}}
namespace xs { namespace table {
struct PreparedCache {
  struct Entry {std::array<uint8_t,32>hash{};std::weak_ptr<const UserTable>table;};
  std::array<Entry,64>entries;size_t next=0;std::mutex nonRealtimeOnly;
};
static PreparedCache&preparedCache(){static PreparedCache cache;return cache;}
static void fft(std::vector<std::complex<double>>&a,bool inverse){
  size_t n=a.size();for(size_t i=1,j=0;i<n;++i){size_t bit=n>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(a[i],a[j]);}
  for(size_t length=2;length<=n;length*=2){double angle=(inverse?2:-2)*Pi/length;std::complex<double>step(std::cos(angle),std::sin(angle));
    for(size_t i=0;i<n;i+=length){std::complex<double>w(1,0);for(size_t j=0;j<length/2;++j){auto u=a[i+j],v=a[i+j+length/2]*w;a[i+j]=u+v;a[i+j+length/2]=u-v;w*=step;}}}
  if(inverse)for(auto&x:a)x/=double(n);
}
std::shared_ptr<const UserTable> prepareUser(std::vector<float>samples,int length,int frames,std::string name,uint32_t rate,uint32_t transforms){
  if(length<16||length>4096||frames<1||frames>256||size_t(length)*frames!=samples.size()||samples.size()>1048576||name.size()>255||!validUtf8(name))throw std::runtime_error("USER dimensions exceed limits");
  for(float x:samples)if(!std::isfinite(x)||std::abs(x)>1)throw std::runtime_error("USER samples outside[-1,1]");
  auto hash=documentState::contentHash(length,frames,samples);auto&cache=preparedCache();
  {std::lock_guard<std::mutex>lock(cache.nonRealtimeOnly);for(auto&e:cache.entries)if(e.hash==hash)if(auto t=e.table.lock())if(t->name==name&&t->sourceRate==rate&&t->transforms==transforms)return t;}
  auto result=std::make_shared<UserTable>();result->name=std::move(name);result->sourceRate=rate;result->transforms=transforms;
  auto&t=result->playback;t.length=length;t.frames=frames;t.rootBin=1;t.pcm=std::move(samples);
  std::copy_n(result->name.c_str(),std::min(result->name.size(),t.name.size()-1),t.name.data());
  int density=256;while(density<length)density*=2;
  for(int h=(length-1)/2;h>0;h/=2)t.levels.push_back({density,h,std::vector<float>(size_t(density)*frames)});
  t.levels.push_back({1,0,std::vector<float>(frames)});
  std::vector<std::complex<double>>spectrum(density),filtered(density);
  for(int frame=0;frame<frames;++frame){
    for(int i=0;i<density;++i){double x=double(i)*length/density;int a=int(x);double f=x-a;const float*p=t.pcm.data()+size_t(frame)*length;spectrum[i]=p[a]+f*(p[(a+1)%length]-p[a]);}
    fft(spectrum,false);
    double canonicalMean=0;for(int i=0;i<length;++i)canonicalMean+=t.pcm[size_t(frame)*length+i];canonicalMean/=length;
    spectrum[0]=canonicalMean*density; // Preserve source DC despite derivative-only periodic resampling.
    for(auto&level:t.levels){if(!level.harmonic){level.samples[frame]=float(spectrum[0].real()/density);continue;}
      std::fill(filtered.begin(),filtered.end(),std::complex<double>{});filtered[0]=spectrum[0];
      for(int k=1;k<=level.harmonic;++k){filtered[k]=spectrum[k];filtered[density-k]=spectrum[density-k];}
      fft(filtered,true);for(int i=0;i<density;++i)level.samples[size_t(frame)*density+i]=float(filtered[i].real());
    }
  }
  {std::lock_guard<std::mutex>lock(cache.nonRealtimeOnly);for(auto&e:cache.entries)if(e.hash==hash)if(auto prior=e.table.lock())if(prior->name==result->name&&prior->sourceRate==rate&&prior->transforms==transforms)return prior;
    auto&e=cache.entries[cache.next++%cache.entries.size()];e.hash=hash;e.table=result;
  }
  return result;
}
}} // namespace xs::table
