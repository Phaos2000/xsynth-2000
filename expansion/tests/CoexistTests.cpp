// GPL-2.0-or-later. Load both products concurrently through their public VST3 ABI.
#include "Core.h"
#include "ProductIdentity.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include <windows.h>
#include <iostream>
#include <stdexcept>
using namespace Steinberg;
using namespace Steinberg::Vst;
void check(bool b,const char *message){if(!b)throw std::runtime_error(message);}
int main(int argc,char **argv) {
 try {
  check(argc==3,"Expected reference and expansion DLLs");
  HostApplication host;HMODULE dll[2]{};IPluginFactory *factory[2]{};IComponent *processor[2]{};IEditController *controller[2]{};IPlugView *view[2]{};
  PClassInfo classes[2][2]{};
  auto parent=CreateWindowW(L"STATIC",L"Dual-product host",WS_POPUP,0,0,1000,800,nullptr,nullptr,nullptr,nullptr);
  for(int i=0;i<2;++i) {
   dll[i]=LoadLibraryA(argv[i+1]);check(dll[i]!=nullptr,"Load both products");
   auto initialize=reinterpret_cast<bool(*)()>(GetProcAddress(dll[i],"InitDll"));if(initialize)check(initialize(),"DLL init");
   auto get=reinterpret_cast<IPluginFactory*(*)()>(GetProcAddress(dll[i],"GetPluginFactory"));check(get!=nullptr,"Factory export");factory[i]=get();
   for(int c=0;c<2;++c)check(factory[i]->getClassInfo(c,&classes[i][c])==kResultOk,"Class identity");
   check(factory[i]->createInstance(classes[i][0].cid,IComponent::iid.toTUID(),reinterpret_cast<void **>(&processor[i]))==kResultOk,"Create processor");
   check(processor[i]->initialize(&host)==kResultOk,"Initialize processor");
   check(factory[i]->createInstance(classes[i][1].cid,IEditController::iid.toTUID(),reinterpret_cast<void **>(&controller[i]))==kResultOk,"Create controller");
   check(controller[i]->initialize(&host)==kResultOk,"Initialize controller");
   view[i]=controller[i]->createView(ViewType::kEditor);check(view[i] && view[i]->attached(parent,kPlatformTypeHWND)==kResultOk,"Attach simultaneous editors");
  }
  for(int i=0;i<2;++i)for(int j=0;j<2;++j)check(std::memcmp(classes[0][i].cid,classes[1][j].cid,16)!=0,"Product class-ID collision");
  check(std::strcmp(classes[0][0].name,classes[1][0].name)!=0,"Product names collide");
  ParameterInfo reference{},expanded{};controller[0]->getParameterInfo(0,reference);controller[1]->getParameterInfo(0,expanded);
  check(reference.id==0 && expanded.id==xs::hostId(0),"Host parameter namespaces overlap");
  MemoryStream referenceUI,expandedUI;controller[0]->getState(&referenceUI);controller[1]->getState(&expandedUI);
  check(std::memcmp(referenceUI.getData(),"XSUI",4)==0 && std::memcmp(expandedUI.getData(),"XSEU",4)==0,"Controller state namespace collision");
  auto preset=xs::encode(xs::defaults());MemoryStream incompatible(preset.data(),preset.size());
  check(processor[0]->setState(&incompatible)!=kResultOk,"Reference accepted expansion state");
  auto referencePreset=xs::defaults();referencePreset[0]=referencePreset[2]=0;
  auto old=xs::encode(referencePreset);old.resize(264);old[2]='V';old[3]='3';old[4]=1;MemoryStream import(old.data(),old.size());
  check(processor[1]->setState(&import)==kResultOk,"Expansion cannot import reference state");
  check(FindWindowExW(parent,nullptr,L"Xsynth1999Editor",nullptr)!=nullptr,"Reference editor class missing");
  check(FindWindowExW(parent,nullptr,L"XsynthExpansionDevEditor",nullptr)!=nullptr,"Expansion editor class missing");
  view[0]->removed();view[0]->release();
  auto survivor=FindWindowExW(parent,nullptr,L"XsynthExpansionDevEditor",nullptr);check(survivor!=nullptr,"Closing reference destroyed expansion editor");SendMessageW(survivor,WM_PAINT,0,0);
  view[1]->removed();view[1]->release();
  for(int i=0;i<2;++i){controller[i]->terminate();controller[i]->release();processor[i]->terminate();processor[i]->release();factory[i]->release();auto shutdown=reinterpret_cast<bool(*)()>(GetProcAddress(dll[i],"ExitDll"));if(shutdown)shutdown();FreeLibrary(dll[i]);}
  DestroyWindow(parent);
  std::cout<<"PASS: both DLLs, processors and editors coexist; class IDs, host parameter IDs and state namespaces are separate; reference import works\n";
  return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
