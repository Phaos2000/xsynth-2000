// GPL-2.0-or-later. Exercise the built VST3 through its public interfaces.
#define NOMINMAX
#include "Core.h"
#include "ProtectState.h"
#include <deque>
#include "ProductIdentity.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include <iostream>
#include <stdexcept>
#include <windows.h>
using namespace Steinberg;
using namespace Steinberg::Vst;
void check(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
}
class EditHandler : public IComponentHandler {
public:
  int begins = 0, edits = 0, ends = 0;
  ParamID lastID = 0;
  ParamValue lastValue = 0;
  tresult PLUGIN_API queryInterface(const TUID id, void **out) override {
    if (FUnknownPrivate::iidEqual(id, IComponentHandler::iid) ||
        FUnknownPrivate::iidEqual(id, FUnknown::iid)) {
      *out = static_cast<IComponentHandler *>(this);
      addRef();
      return kResultOk;
    }
    *out = nullptr;
    return kNoInterface;
  }
  uint32 PLUGIN_API addRef() override { return 1; }
  uint32 PLUGIN_API release() override { return 1; }
  tresult PLUGIN_API beginEdit(ParamID id) override {
    ++begins;
    lastID = id;
    return kResultOk;
  }
  tresult PLUGIN_API performEdit(ParamID id, ParamValue v) override {
    ++edits;
    lastID = id;
    lastValue = v;
    return kResultOk;
  }
  tresult PLUGIN_API endEdit(ParamID id) override {
    ++ends;
    lastID = id;
    return kResultOk;
  }
  tresult PLUGIN_API restartComponent(int32) override { return kResultOk; }
};
int main(int argc, char **argv) {
  try {
    check(argc == 2, "plugin argument");
    auto dll = LoadLibraryA(argv[1]);
    check(dll != nullptr, "load DLL");
    auto init = reinterpret_cast<bool (*)()>(GetProcAddress(dll, "InitDll"));
    if (init)
      check(init(), "InitDll");
    auto getFactory =
        reinterpret_cast<IPluginFactory *(*)()>(GetProcAddress(dll, "GetPluginFactory"));
    check(getFactory != nullptr, "factory export");
    auto factory = getFactory();
    PFactoryInfo factoryInfo{};
    check(factory->getFactoryInfo(&factoryInfo)==kResultOk && std::strcmp(factoryInfo.vendor,"Phaos")==0,"Phaos vendor identity");
    HostApplication host;
    PClassInfo info{};
    factory->getClassInfo(0, &info);
    check(std::strcmp(info.name,"Xsynth 2000")==0,"Xsynth 2000 processor name");
    IComponent *component{};
    check(factory->createInstance(info.cid, IComponent::iid.toTUID(),
                                  reinterpret_cast<void **>(&component)) == kResultOk,
          "processor");
    check(component->initialize(&host) == kResultOk, "initialize");
    IAudioProcessor *processor{};
    check(component->queryInterface(IAudioProcessor::iid.toTUID(),
                                    reinterpret_cast<void **>(&processor)) == kResultOk,
          "audio interface");
    factory->getClassInfo(1, &info);
    check(std::strcmp(info.name,"Xsynth 2000 Controller")==0,"Xsynth 2000 controller name");
    IEditController *controller{};
    check(factory->createInstance(info.cid, IEditController::iid.toTUID(),
                                  reinterpret_cast<void **>(&controller)) == kResultOk,
          "controller");
    check(controller->initialize(&host) == kResultOk, "controller initialize");
    auto state = xs::defaults();
    state[0] = .35;
    state[24] = .4;
    state[27] = 1;
    auto encoded = xs::encode(state);
    MemoryStream input(encoded.data(), encoded.size());
    check(component->setState(&input) == kResultOk, "restore processor");
    input.seek(0, IBStream::kIBSeekSet, nullptr);
    check(controller->setComponentState(&input) == kResultOk, "restore controller");
    for (int i = 0; i < xs::Count; ++i)
      check(controller->getParamNormalized(xs::hostId(i)) == state[i], "controller recall");
    MemoryStream saved;
    check(component->getState(&saved) == kResultOk, "save processor");
    auto hostEncoded=xs::protect::encodeHost(state,{},false);
    check(saved.getSize() == hostEncoded.size() &&
              std::memcmp(saved.getData(), hostEncoded.data(), hostEncoded.size()) == 0,
          "serialized recall");
    ProcessSetup setup{};
    setup.processMode = kOffline;
    setup.symbolicSampleSize = kSample32;
    setup.maxSamplesPerBlock = 512;
    setup.sampleRate = 44100;
    check(processor->setupProcessing(setup) == kResultOk, "setup");
    const int latency=int(processor->getLatencySamples());check(latency==76,"44100Hz protection latency");
    component->activateBus(kAudio, kOutput, 0, true);
    component->activateBus(kEvent, kInput, 0, true);
    auto render = [&](int blockSize) {
      component->setActive(true);
      processor->setProcessing(true);
      std::vector<float> out(2048);
      float l[512]{}, r[512]{};
      float *channels[] = {l, r};
      AudioBusBuffers bus{};
      bus.numChannels = 2;
      bus.channelBuffers32 = channels;
      for (int offset = 0; offset < 2048; offset += blockSize) {
        int n = std::min(blockSize, 2048 - offset);
        EventList events;
        Event e{};
        e.type = Event::kNoteOnEvent;
        e.noteOn.pitch = 60;
        e.noteOn.velocity = 1;
        if (offset <= 37 && offset + n > 37) {
          e.sampleOffset = 37 - offset;
          events.addEvent(e);
        }
        ParameterChanges changes;
        int32 index = 0;
        for (auto time : {700, 900})
          if (offset <= time && offset + n > time) {
            auto q = changes.addParameterData(xs::hostId(29), index);
            q->addPoint(time - offset, time == 700 ? 0 : 1, index);
          }
        ProcessData data{};
        data.processMode = kOffline;
        data.symbolicSampleSize = kSample32;
        data.numSamples = n;
        data.numOutputs = 1;
        data.outputs = &bus;
        data.inputEvents = &events;
        data.inputParameterChanges = &changes;
        check(processor->process(data) == kResultOk, "process");
        for (int i = 0; i < n; ++i) {
          check(l[i] == r[i], "dual mono");
          out[offset + i] = l[i];
        }
      }
      processor->setProcessing(false);
      component->setActive(false);
      return out;
    };
    auto a = render(512);
    auto b = render(17);
    check(a == b, "host block independence");
    double energy = 0;
    for (int i = 0; i < 2048; ++i) {
      if (i < 37+latency || (i >= 700+latency && i < 900+latency))
        check(a[i] == 0, "sample accurate gate/event");
      energy += a[i] * a[i];
    }
    check(energy > 1, "render energy");
    check(controller->getParameterCount()==xs::Count,"Expanded host parameter count");
    for(int id=34;id<xs::Count;++id) {
      ParameterInfo parameter{};check(controller->getParameterInfo(id,parameter)==kResultOk && parameter.id==xs::hostId(id),"Stable expansion parameter IDs");
      check(parameter.stepCount==xs::stepsFor(id),"Discrete step semantics");
      check(controller->setParamNormalized(xs::hostId(id),.723)==kResultOk,"Controller accepts expansion parameter");
      check(controller->getParamNormalized(xs::hostId(id))==xs::stepped(id,.723),"Controller quantizes selectors");
    }
    IMidiMapping *mapping{};
    check(controller->queryInterface(IMidiMapping::iid.toTUID(),reinterpret_cast<void **>(&mapping))==kResultOk,"MIDI mapping interface");
    ParamID pressure{};check(mapping->getMidiControllerAssignment(0,0,kAfterTouch,pressure)==kResultTrue && pressure==xs::hostId(xs::Pressure),"Pressure source mapping");
    for(int channel=0;channel<16;++channel)for(int cc:{64,66}){ParamID pedal{};check(mapping->getMidiControllerAssignment(0,int16(channel),CtrlNumber(cc),pedal)==kResultTrue&&pedal==xs::hostId(cc==64?xs::Sustain:xs::Sostenuto),"Pedal MIDI mapping");}mapping->release();
    {
      auto initial=xs::defaults();auto serialized=xs::encode(initial);
      MemoryStream restored(serialized.data(),serialized.size());
      check(component->setState(&restored)==kResultOk,"Restore automation fixture");
      component->setActive(true);processor->setProcessing(true);
      float left[512]{},right[512]{};float *channels[]={left,right};AudioBusBuffers bus{};bus.numChannels=2;bus.channelBuffers32=channels;
      EventList events;Event event{};event.type=Event::kNoteOnEvent;event.noteOn.pitch=60;event.noteOn.velocity=1;events.addEvent(event);
      ParameterChanges changes;int32 index=0;changes.addParameterData(xs::hostId(23),index)->addPoint(511,0,index);
      ProcessData data{};data.numSamples=512;data.symbolicSampleSize=kSample32;data.numOutputs=1;data.outputs=&bus;data.inputEvents=&events;data.inputParameterChanges=&changes;
      check(processor->process(data)==kResultOk,"Continuous automation process");
      xs::Engine expected;expected.reset(44100);
      std::deque<float> delayed(latency,0);
      for(int i=0;i<512;++i){expected.set(23,.5*(511-i)/512.);if(i==0)expected.note(60,true);delayed.push_back(expected.next());check(std::abs(left[i]-delayed.front())<1.e-7,"VST3 implicit -1 point ramp mismatch after PDC delay");delayed.pop_front();}
      data.numSamples=latency+1;data.inputEvents=nullptr;data.inputParameterChanges=nullptr;
      check(processor->process(data)==kResultOk,"Continuous automation drain");
      for(int i=0;i<latency+1;++i){delayed.push_back(expected.next());check(std::abs(left[i]-delayed.front())<1.e-7,"Delayed ramp drain");delayed.pop_front();}
      check(left[latency-1]==0&&left[latency]==0,"Continuous ramp final point after PDC delay");
      processor->setProcessing(false);component->setActive(false);
      // Controller values used by the GUI remain an independent host-owned view.
      MemoryStream ui(serialized.data(),serialized.size());controller->setComponentState(&ui);
    }
    // Exercise public VST3 queues, not merely state migration or engine setters.
    for(int slot=0;slot<6;++slot)for(int block:{1,7,32,128,512})for(int expanded=0;expanded<2;++expanded)
      for(int destination=0;destination<(expanded?41:18);++destination){
        auto initial=xs::defaults();initial[xs::VoiceMode]=1.;initial[23]=.1;
        initial[xs::MatrixFirst+slot*3]=1.;initial[xs::MatrixFirst+slot*3+2]=.6;
        auto bytes=xs::encode(initial);MemoryStream restored(bytes.data(),bytes.size());
        check(component->setState(&restored)==kResultOk,"Modern fixture restore");
        component->setActive(true);processor->setProcessing(true);
        xs::Engine expected;expected.state=initial;expected.reset(44100);
        std::deque<float> delayed(latency,0);
        int legacy=xs::legacyDestinationId(slot),extension=xs::ExpandedDestinationFirst+slot;
        for(int pass=0;pass<3+(latency+block-1)/block;++pass){
          ParameterChanges changes;int32 point=0;
          // Insert legacy queue first: equal-offset precedence must be independent of queue order.
          if(!expanded)changes.addParameterData(xs::hostId(legacy),point)->addPoint(block-1,destination/17.,point);
          changes.addParameterData(xs::hostId(extension),point)->addPoint(0,expanded?(destination+1)/41.:1.,point);
          EventList events;if(pass==0){Event event{};event.type=Event::kNoteOnEvent;event.noteOn.pitch=60;event.noteOn.velocity=.7;events.addEvent(event);}
          float l[512]{},r[512]{};float *channels[]={l,r};AudioBusBuffers bus{};bus.numChannels=2;bus.channelBuffers32=channels;
          ProcessData data{};data.numSamples=block;data.symbolicSampleSize=kSample32;data.numOutputs=1;data.outputs=&bus;data.inputEvents=&events;data.inputParameterChanges=&changes;
          check(processor->process(data)==kResultOk,"Destination host automation processing");
          for(int sample=0;sample<block;++sample){
            if(sample==0)expected.set(extension,expanded?(destination+1)/41.:1.);
            if(!expanded && sample==block-1)expected.set(legacy,destination/17.);
            if(pass==0&&sample==0)expected.note(60,true,.7);
            delayed.push_back(expected.next());
            check(std::abs(l[sample]-delayed.front())<1.e-7,"Destination queue timing/audio mismatch after PDC delay");delayed.pop_front();
          }
          MemoryStream saved;component->getState(&saved);xs::State actual{};
          check(xs::decode(reinterpret_cast<const uint8_t*>(saved.getData()),saved.getSize(),actual),"Automation state read");
          check(xs::effectiveDestination(actual,slot)==destination,"Public VST3 automation destination meaning");
        }
        processor->setProcessing(false);component->setActive(false);
        controller->setParamNormalized(xs::hostId(extension),1.);
        controller->setParamNormalized(xs::hostId(legacy),destination%18/17.);
        check(controller->getParamNormalized(xs::hostId(extension))==0.,"Controller legacy takeover");
      }
    std::cout<<"PASS public VST3 queues: six slots, all 18 legacy and 41 expanded choices, five block sizes, repeats, sample timing, same-offset precedence\n";
    for(int slot=0;slot<6;++slot)for(int takeover=0;takeover<3;++takeover){
      auto initial=xs::defaults();initial[xs::Unison]=1.;initial[xs::Detune]=.5;initial[xs::VoiceMode]=1.;
      initial[xs::MatrixFirst+slot*3]=1.;initial[xs::MatrixFirst+slot*3+2]=.6;
      auto bytes=xs::encode(initial);MemoryStream restored(bytes.data(),bytes.size());component->setState(&restored);component->setActive(true);processor->setProcessing(true);
      ParameterChanges changes;int32 point=0;changes.addParameterData(xs::hostId(xs::DetuneRouteFirst+slot),point)->addPoint(0,1.,point);
      if(takeover)changes.addParameterData(xs::hostId(takeover==1?xs::ExpandedDestinationFirst+slot:xs::legacyDestinationId(slot)),point)->addPoint(0,takeover==1?41/41.:17/17.,point);
      EventList events;Event event{};event.type=Event::kNoteOnEvent;event.noteOn.pitch=60;event.noteOn.velocity=.7;event.noteOn.noteId=17;events.addEvent(event);
      float l[128]{},r[128]{};float *channels[]={l,r};AudioBusBuffers bus{};bus.numChannels=2;bus.channelBuffers32=channels;ProcessData data{};data.numSamples=128;data.symbolicSampleSize=kSample32;data.numOutputs=1;data.outputs=&bus;data.inputEvents=&events;data.inputParameterChanges=&changes;check(processor->process(data)==kResultOk,"New destination queue processing");
      MemoryStream saved;component->getState(&saved);xs::State actual{};check(xs::decode(reinterpret_cast<const uint8_t*>(saved.getData()),saved.getSize(),actual),"New destination queue state");check(xs::effectiveDestination(actual,slot)==(takeover==0?41:takeover==1?40:17),"Appended destination precedence");
      for(int i=0;i<128;++i)check(std::isfinite(l[i])&&l[i]==r[i],"Unison mono output mismatch");
      ParameterChanges stop;stop.addParameterData(xs::hostId(xs::AllSoundsOff),point)->addPoint(0,1,point);data.inputParameterChanges=&stop;data.inputEvents=nullptr;processor->process(data);for(float x:l)check(x==0,"Unison All Sounds Off did not clear");
      controller->setParamNormalized(xs::hostId(xs::DetuneRouteFirst+slot),1.);controller->setParamNormalized(xs::hostId(xs::ExpandedDestinationFirst+slot),1.);check(controller->getParamNormalized(xs::hostId(xs::DetuneRouteFirst+slot))==0.,"Controller appended route takeover");
      processor->setProcessing(false);component->setActive(false);
    }
    for(int slot=0;slot<xs::MatrixSlots;++slot)for(int source=0;source<xs::SourceCount;++source)for(int order=0;order<2;++order){
      auto initial=xs::defaults();initial[xs::matrixId(slot)]=.1;auto bytes=xs::encode(initial);MemoryStream restored(bytes.data(),bytes.size());component->setState(&restored);component->setActive(true);processor->setProcessing(true);
      ParameterChanges changes;int32 point=0;int sourceId=xs::matrixId(slot),extension=xs::SourceExtensionFirst+slot;
      auto legacy=[&]{changes.addParameterData(xs::hostId(sourceId),point)->addPoint(0,source<11?source/10.:.1,point);};
      auto ext=[&]{changes.addParameterData(xs::hostId(extension),point)->addPoint(0,source<11?1.:(source-10)/3.,point);};
      if(source<11){if(order){legacy();ext();}else{ext();legacy();}}else ext();
      float l[32]{},r[32]{};float *channels[]={l,r};AudioBusBuffers bus{};bus.numChannels=2;bus.channelBuffers32=channels;ProcessData data{};data.numSamples=32;data.symbolicSampleSize=kSample32;data.numOutputs=1;data.outputs=&bus;data.inputParameterChanges=&changes;check(processor->process(data)==kResultOk,"Source queue processing");
      MemoryStream saved;component->getState(&saved);xs::State actual{};check(xs::decode(reinterpret_cast<const uint8_t*>(saved.getData()),saved.getSize(),actual),"Source queue state");check(xs::effectiveSource(actual,slot)==source,"Historical source /10 or extension precedence changed");
      controller->setParamNormalized(xs::hostId(extension),1);controller->setParamNormalized(xs::hostId(sourceId),.1);check(controller->getParamNormalized(xs::hostId(extension))==0,"Controller source takeover");processor->setProcessing(false);component->setActive(false);
    }
    std::cout<<"PASS public source queues: 12 slots x 14 sources, both insertion orders, historical /10 writes take precedence including repeats\n";
    auto view = controller->createView(ViewType::kEditor);
    check(view != nullptr, "editor interface");
    EditHandler handler;
    controller->setComponentHandler(&handler);
    IPlugViewContentScaleSupport *scaling{};
    check(view->queryInterface(IPlugViewContentScaleSupport::iid.toTUID(),
                               reinterpret_cast<void **>(&scaling)) == kResultOk,
          "DPI interface");
    auto parent = CreateWindowW(L"STATIC", L"Xsynth integration host", WS_POPUP, 0, 0, 1000, 1146,
                                nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    check(parent != nullptr, "hidden host window");
    for (int pass = 0; pass < 3; ++pass) {
      check(view->attached(parent, kPlatformTypeHWND) == kResultOk, "attach editor");
      for (float factor : {1.f, 1.25f, 1.5f, 2.f, 1.f}) {
        check(scaling->setContentScaleFactor(factor) == kResultOk, "content scaling");
        ViewRect size;
        view->getSize(&size);
        check(size.getWidth() == int(std::round(xs::NativeWidth * factor)) && size.getHeight() == int(std::round(xs::NativeHeight * factor)),
              "scaled view size");
      }
      auto child = GetWindow(parent, GW_CHILD);
      check(child != nullptr, "native editor window");
      SendMessageW(child, WM_PAINT, 0, 0);
      auto initial = controller->getParamNormalized(xs::hostId(0));
      int before = handler.begins;
      SendMessageW(child, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(39, 93));
      SendMessageW(child, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(39, 78));
      SendMessageW(child, WM_LBUTTONUP, 0, MAKELPARAM(39, 78));
      check(handler.begins == before + 1 && handler.ends == handler.begins && handler.edits > 0,
            "balanced host edit gesture");
      check(handler.lastID == xs::hostId(0) &&
                std::abs(handler.lastValue - std::min(1., std::round((initial + .1)*48)/48)) < 1.e-10,
            "knob drag reaches host");
      check(view->removed() == kResultOk, "remove editor");
      check(GetWindow(parent, GW_CHILD) == nullptr, "native editor cleanup");
    }
    DestroyWindow(parent);
    scaling->release();
    view->release();
    controller->setComponentHandler(nullptr);
    controller->terminate();
    controller->release();
    processor->release();
    component->terminate();
    component->release();
    factory->release();
    auto exit = reinterpret_cast<bool (*)()>(GetProcAddress(dll, "ExitDll"));
    if (exit)
      exit();
    FreeLibrary(dll);
    std::cout << "Passed: built VST3 state recall, controller recall, MIDI "
                 "offset, automation offsets, stereo, editor attach/detach, DPI, "
                 "host edit gestures, block sizes 17 and 512\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}

