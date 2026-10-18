// GPL-2.0-or-later. Exercise the built VST3 through its public interfaces.
#define NOMINMAX
#include "Core.h"
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
    check(saved.getSize() == encoded.size() &&
              std::memcmp(saved.getData(), encoded.data(), encoded.size()) == 0,
          "serialized recall");
    ProcessSetup setup{};
    setup.processMode = kOffline;
    setup.symbolicSampleSize = kSample32;
    setup.maxSamplesPerBlock = 512;
    setup.sampleRate = 44100;
    check(processor->setupProcessing(setup) == kResultOk, "setup");
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
      if (i < 37 || (i >= 700 && i < 900))
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
    ParamID pressure{};check(mapping->getMidiControllerAssignment(0,0,kAfterTouch,pressure)==kResultTrue && pressure==xs::hostId(xs::Pressure),"Pressure source mapping");mapping->release();
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
      for(int i=0;i<512;++i){expected.set(23,.5*(511-i)/512.);if(i==0)expected.note(60,true);check(std::abs(left[i]-expected.next())<1.e-7,"VST3 implicit -1 point ramp mismatch");}
      check(left[511]==0,"Continuous ramp final point");
      processor->setProcessing(false);component->setActive(false);
      // Controller values used by the GUI remain an independent host-owned view.
      MemoryStream ui(serialized.data(),serialized.size());controller->setComponentState(&ui);
    }
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
