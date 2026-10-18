// GPL-2.0-or-later. Actual VST3 process wrapper/control-only timing.
#define NOMINMAX
#include "Core.h"
#include "ProductIdentity.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include <windows.h>
#include <chrono>
#include <fstream>
#include <iostream>
using namespace Steinberg;using namespace Steinberg::Vst;
int main(int argc,char**argv){if(argc!=3)return 1;auto dll=LoadLibraryA(argv[1]);if(!dll)return 2;auto init=reinterpret_cast<bool(*)()>(GetProcAddress(dll,"InitDll"));if(init&&!init())return 3;auto get=reinterpret_cast<IPluginFactory*(*)()>(GetProcAddress(dll,"GetPluginFactory"));auto*f=get();PClassInfo info{};f->getClassInfo(0,&info);IComponent*c=nullptr;f->createInstance(info.cid,IComponent::iid.toTUID(),reinterpret_cast<void**>(&c));HostApplication host;c->initialize(&host);IAudioProcessor*p=nullptr;c->queryInterface(IAudioProcessor::iid.toTUID(),reinterpret_cast<void**>(&p));ProcessSetup setup{};setup.sampleRate=96000;setup.maxSamplesPerBlock=1024;setup.symbolicSampleSize=kSample32;setup.processMode=kRealtime;p->setupProcessing(setup);c->setActive(true);p->setProcessing(true);
 std::ofstream csv(argv[2]);csv<<"work,block,median_us_per_call,ms_per_audio_second_at_96k\n";
 for(int mode=0;mode<3;++mode)for(int block:{32,64,128,256,512,1024}){ParameterChanges changes;int32 index=0;if(mode==2){auto*q=changes.addParameterData(xs::hostId(23),index);q->addPoint(0,.15,index);}ProcessData data{};data.symbolicSampleSize=kSample32;data.processMode=kRealtime;data.numSamples=mode==1?block:0;if(mode==2)data.inputParameterChanges=&changes;for(int n=0;n<256;++n)p->process(data);std::array<double,5>times{};for(auto&t:times){auto start=std::chrono::steady_clock::now();for(int n=0;n<10000;++n)p->process(data);t=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/10000;}std::sort(times.begin(),times.end());csv<<(mode==0?"control-sync-empty":mode==1?"silent-render-and-wrapper":"control-one-repeated-queue")<<','<<block<<','<<times[2]<<','<<times[2]*96/block<<'\n';}
 p->setProcessing(false);c->setActive(false);p->release();c->terminate();c->release();f->release();auto exit=reinterpret_cast<bool(*)()>(GetProcAddress(dll,"ExitDll"));if(exit)exit();FreeLibrary(dll);return 0;
}
