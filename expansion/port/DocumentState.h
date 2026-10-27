// GPL-2.0-or-later. Included inside xs after State and historical decoder.
namespace documentState {
inline uint32_t u32(const uint8_t*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline void put(std::vector<uint8_t>&b,uint32_t n){for(int i=0;i<4;++i)b.push_back(uint8_t(n>>(i*8)));}
inline void chunk(std::vector<uint8_t>&b,const char*name,const std::vector<uint8_t>&p){b.insert(b.end(),name,name+4);put(b,uint32_t(p.size()));b.insert(b.end(),p.begin(),p.end());while(b.size()%4)b.push_back(0);}
// Defined in DocumentAssets.cpp, off the audio path, using Windows SHA-256.
std::array<uint8_t,32> contentHash(int n,int frames,const std::vector<float>&samples);
}
inline std::vector<uint8_t> encodeDocument(const State&s,const table::Document&doc){
  using namespace documentState;
  const int parameterCount=storedParameterCount(s);
  std::vector<uint8_t>b={'X','S','E','D'};put(b,7);put(b,32);put(b,0);put(b,parameterCount);put(b,0);put(b,0);put(b,0);
  std::vector<uint8_t>params;
  for(int i=0;i<parameterCount;++i){double v=s[i];uint64_t bits;std::memcpy(&bits,&v,8);for(int j=0;j<8;++j)params.push_back(uint8_t(bits>>(j*8)));}
  chunk(b,"PARM",params);
  std::vector<std::array<uint8_t,32>>hashes;
  std::vector<const table::UserTable*>unique;
  std::vector<uint8_t>map;
  size_t sampleBytes=0;
  for(int slot=0;slot<int(table::UserSlots);++slot)if(auto t=doc.slots[slot].get()){
    auto hash=contentHash(t->playback.length,t->playback.frames,t->playback.pcm);
    put(map,slot);map.insert(map.end(),hash.begin(),hash.end());
    auto found=std::find(hashes.begin(),hashes.end(),hash);
    if(found==hashes.end()){hashes.push_back(hash);unique.push_back(t);sampleBytes+=t->playback.pcm.size()*4;}
  }
  if(sampleBytes>table::MaxCanonicalBytes)throw std::runtime_error("USER payload exceeds8MiB");
  chunk(b,"UMAP",map);
  for(size_t i=0;i<unique.size();++i){auto&t=*unique[i];const auto&w=t.playback;std::vector<uint8_t>p(hashes[i].begin(),hashes[i].end());
    put(p,1);put(p,w.length);put(p,w.frames);put(p,t.sourceRate);put(p,t.transforms);put(p,uint32_t(t.name.size()));
    p.insert(p.end(),t.name.begin(),t.name.end());
    for(float x:w.pcm){uint32_t v;std::memcpy(&v,&x,4);put(p,v);}chunk(b,"UTBL",p);
  }
  if(b.size()>table::MaxStateBytes)throw std::runtime_error("state exceeds native cap");
  uint32_t total=uint32_t(b.size()),chunks=uint32_t(unique.size()+2);
  for(int i=0;i<4;++i){b[12+i]=uint8_t(total>>(i*8));b[20+i]=uint8_t(chunks>>(i*8));}
  return b;
}
inline std::vector<uint8_t> encode(const State&s){return encodeDocument(s,{});}
inline bool decodeDocument(const uint8_t*b,size_t size,State&out,table::Document&assets){
  using namespace documentState;
  if(size<8)return false;
  if(std::memcmp(b,"XSED\7\0\0\0",8)){State s;if(!decodeHistorical(b,size,s))return false;out=s;assets={};return true;}
  try{
    if(size<32||size>table::MaxStateBytes||u32(b+8)!=32||u32(b+12)!=size||(u32(b+16)!=Count&&u32(b+16)!=PreDriveCount&&u32(b+16)!=PreVoicesCount&&u32(b+16)!=PreDistortionCount&&u32(b+16)!=PreWidthCount&&u32(b+16)!=PrePedalCount&&u32(b+16)!=PreDualCount)||u32(b+24)||u32(b+28))return false;
    const int parameterCount=int(u32(b+16)); // Original XSED7 defaults appended WIDTH to0.
    uint32_t count=u32(b+20);if(count<2||count>19)return false;
    State s=defaults();table::Document doc;bool parm=false,mapSeen=false;
    std::vector<std::pair<int,std::array<uint8_t,32>>> mappings;
    std::vector<std::pair<std::array<uint8_t,32>,std::shared_ptr<const table::UserTable>>>tables;
    size_t at=32,sampleBytes=0,metadata=0;
    for(uint32_t c=0;c<count;++c){
      if(size-at<8)return false;const uint8_t*type=b+at;size_t len=u32(b+at+4);at+=8;
      if(len>size-at)return false;const uint8_t*p=b+at;
      if(!std::memcmp(type,"PARM",4)){
        if(parm||len!=parameterCount*8)return false;parm=true;
        for(int i=0;i<parameterCount;++i){uint64_t bits=0;for(int j=0;j<8;++j)bits|=uint64_t(p[i*8+j])<<(j*8);std::memcpy(&s[i],&bits,8);if(!valid(s[i]))return false;}
      }else if(!std::memcmp(type,"UMAP",4)){
        if(mapSeen||len%36||len>16*36)return false;mapSeen=true;unsigned used=0;
        for(size_t pos=0;pos<len;pos+=36){int slot=int(u32(p+pos));if(slot<0||slot>=16||(used&(1u<<slot)))return false;used|=1u<<slot;
          std::array<uint8_t,32> hash;std::copy_n(p+pos+4,32,hash.begin());mappings.emplace_back(slot,hash);}
      }else if(!std::memcmp(type,"UTBL",4)){
        if(len<56||tables.size()>=16)return false;
        std::array<uint8_t,32>hash;std::copy_n(p,32,hash.begin());for(auto&t:tables)if(t.first==hash)return false;
        uint32_t version=u32(p+32),n=u32(p+36),frames=u32(p+40),rate=u32(p+44),transforms=u32(p+48),nameLen=u32(p+52);
        if(version!=1||n<16||n>4096||frames<1||frames>256||uint64_t(n)*frames>1048576||nameLen>255||transforms>3)return false;
        size_t bytes=size_t(n)*frames*4;if(len!=56+nameLen+bytes||sampleBytes+bytes>table::MaxCanonicalBytes)return false;
        sampleBytes+=bytes;metadata+=56+nameLen;if(metadata>65536)return false;
        std::string name(reinterpret_cast<const char*>(p+56),nameLen);if(name.find('\0')!=std::string::npos)return false;
        std::vector<float>samples(size_t(n)*frames);
        for(size_t j=0;j<samples.size();++j){uint32_t bits=u32(p+56+nameLen+j*4);std::memcpy(&samples[j],&bits,4);if(!std::isfinite(samples[j])||std::abs(samples[j])>1)return false;}
        if(contentHash(n,frames,samples)!=hash)return false;
        tables.emplace_back(hash,table::prepareUser(std::move(samples),n,frames,std::move(name),rate,transforms));
      }else if(!(type[0]&0x80))return false;
      at+=len;while(at%4){if(at>=size||b[at++]!=0)return false;}
    }
    if(at!=size||!parm||!mapSeen)return false;
    unsigned referenced=0;
    for(auto&m:mappings){bool found=false;for(size_t i=0;i<tables.size();++i)if(m.second==tables[i].first){doc.slots[m.first]=tables[i].second;referenced|=1u<<i;found=true;break;}if(!found)return false;}
    if(referenced!=(1u<<tables.size())-1)return false;
    // An intentionally empty slot is valid and silent. Every UMAP reference,
    // however, must resolve to its hash-verified UTBL above.
    out=s;assets=std::move(doc);return true;
  }catch(...){return false;}
}
inline bool decode(const uint8_t*b,size_t size,State&out){table::Document doc;return decodeDocument(b,size,out,doc);}
