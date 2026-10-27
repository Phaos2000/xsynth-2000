// GPL-2.0-or-later. Prepared USER data are owned and prepared only off render.
#pragma once
#include "TableReader.h"
#include <memory>
#include <string>
namespace xs { namespace table {
constexpr size_t UserSlots=16,MaxCanonicalBytes=8*1024*1024,MaxStateBytes=8519680;
inline bool validUtf8(const std::string&text){
  size_t i=0;while(i<text.size()){uint8_t first=uint8_t(text[i++]);if(!first)return false;if(first<128)continue;int continuation=first>=0xc2&&first<=0xdf?1:first>=0xe0&&first<=0xef?2:first>=0xf0&&first<=0xf4?3:-1;if(continuation<0||text.size()-i<size_t(continuation))return false;
    uint32_t code=first&((1u<<(6-continuation))-1);for(int j=0;j<continuation;++j){uint8_t b=uint8_t(text[i++]);if((b&0xc0)!=0x80)return false;code=(code<<6)|(b&63);}if((continuation==1&&code<128)||(continuation==2&&code<2048)||(continuation==3&&code<65536)||(code>=0xd800&&code<=0xdfff)||code>0x10ffff)return false;
  }return true;
}
struct UserTable {
  Table playback;
  std::string name;
  uint32_t sourceRate=0,transforms=0;
};
struct Document {std::array<std::shared_ptr<const UserTable>,UserSlots> slots{};};
const Bank& factoryBank(); // Constructed before audio starts; never lazy-called by render.
std::shared_ptr<const UserTable> prepareUser(std::vector<float> samples,int length,int frames,std::string name,uint32_t sourceRate=0,uint32_t transforms=0);
}} // namespace xs::table
