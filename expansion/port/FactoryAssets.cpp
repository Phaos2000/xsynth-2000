// GPL-2.0-or-later. Shared prepared data derived solely from canonical PCM8.
#include "DocumentAssets.h"
#include <streambuf>
namespace xs { namespace table {
namespace {
const unsigned char bytes[]={
#include "../assets/PreparedFactory.inc"
};
struct Buffer:std::streambuf{Buffer(){auto p=reinterpret_cast<char*>(const_cast<unsigned char*>(bytes));setg(p,p,p+sizeof bytes);}};
}
const Bank& factoryBank(){static const Bank bank=[](){Buffer buffer;std::istream stream(&buffer);return Bank(stream);}();return bank;}
}}
