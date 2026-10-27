// GPL-2.0-or-later. Host-only optional instance configuration.
#pragma once
namespace xs::protect {
inline std::vector<uint8_t> encodeHost(const State&s,const table::Document&doc,bool enabled){
 auto b=encodeDocument(s,doc);
 const char tag[]={char(0xd0),'R','T','C'};
 documentState::chunk(b,tag,{1,uint8_t(enabled),0,0});
 uint32_t count=documentState::u32(b.data()+20)+1,total=uint32_t(b.size());
 for(int i=0;i<4;++i){b[12+i]=uint8_t(total>>(8*i));b[20+i]=uint8_t(count>>(8*i));}
 return b;
}
// Call after decodeDocument validation. Missing metadata always means legacy OFF.
inline bool readHost(const std::vector<uint8_t>&b,bool&enabled){
 enabled=false;
 if(b.size()<32||std::memcmp(b.data(),"XSED\7\0\0\0",8))return true;
 bool found=false;size_t at=32;
 for(uint32_t i=0;i<documentState::u32(b.data()+20);++i){
  if(at+8>b.size())return false;
  size_t len=documentState::u32(b.data()+at+4),payload=at+8;
  if(len>b.size()-payload)return false;
  if(b[at]==0xd0&&!std::memcmp(b.data()+at+1,"RTC",3)){
   if(found||len!=4||b[payload]!=1||b[payload+1]>1||b[payload+2]||b[payload+3])return false;
   found=true;enabled=b[payload+1]!=0;
  }
  at=(payload+len+3)&~size_t(3);
 }
 return at==b.size();
}
}
