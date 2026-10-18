// GPL-2.0-or-later. Original Xsynth (C) 1999 S. J. Brookes.
#include "Core.h"
#include "Editor.h"
#include "Version.h"
#include "StateExchange.h"
#include "ProductIdentity.h"
#include <xmmintrin.h>
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "public.sdk/source/common/pluginview.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
constexpr ParamID AllNotesOff = xs::AllNotesOff, AllSoundsOff = xs::AllSoundsOff;
constexpr int HostParameterCount = xs::Count;
const FUID ProcessorID(0x7923268C, 0x1C9D43DE, 0xAA65DAAC, 0x78C5232D);
const FUID ControllerID(0x0C0B572A, 0x7CD34DC5, 0x95F21C41, 0x22650E26);
bool load(IBStream *s, xs::State &state) {
  std::array<uint8_t, xs::StateBytes> b{};
  int32 n=0;
  if(!s || s->read(b.data(),8,&n)!=kResultOk || n!=8 || (std::memcmp(b.data(),"XSED",4) && std::memcmp(b.data(),"XSV3",4)))return false;
  int count=xs::stateValueCount(b.data());
  if(!count)return false;
  int32 bytes=count*8;
  return s->read(b.data()+8,bytes,&n)==kResultOk && n==bytes && xs::decode(b.data(),8+bytes,state);
}
struct DenormalGuard {
 unsigned previous=_mm_getcsr();
 DenormalGuard(){_mm_setcsr(previous|0x8040);}
 ~DenormalGuard(){_mm_setcsr(previous);}
};
class Processor : public AudioEffect {
  xs::Engine engine;
  bool wasPlaying = false;
  xs::StateExchange states;
  uint64_t stateGeneration=0;

public:
  Processor() { setControllerClass(ControllerID); }
  uint32 PLUGIN_API getProcessContextRequirements() override {
    return IProcessContextRequirements::kNeedTempo | IProcessContextRequirements::kNeedProjectTimeMusic |
           IProcessContextRequirements::kNeedTransportState;
  }
  static FUnknown *create(void *) { return static_cast<IAudioProcessor *>(new Processor); }
  tresult PLUGIN_API initialize(FUnknown *c) override {
    auto r = AudioEffect::initialize(c);
    if (r != kResultOk)
      return r;
    addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);
    addEventInput(STR16("MIDI In"), 1);
    return kResultOk;
  }
  tresult PLUGIN_API setBusArrangements(SpeakerArrangement *in, int32 ni, SpeakerArrangement *out,
                                        int32 no) override {
    if (ni != 0 || no != 1 || (out[0] != SpeakerArr::kStereo && out[0] != SpeakerArr::kMono))
      return kResultFalse;
    return AudioEffect::setBusArrangements(in, ni, out, no);
  }
  tresult PLUGIN_API canProcessSampleSize(int32 size) override {
    return size == kSample32 || size == kSample64 ? kResultTrue : kResultFalse;
  }
  tresult PLUGIN_API setupProcessing(ProcessSetup &s) override {
    if (!std::isfinite(s.sampleRate) || s.sampleRate < 1)
      return kResultFalse;
    engine.reset(s.sampleRate);
    return AudioEffect::setupProcessing(s);
  }
  tresult PLUGIN_API setActive(TBool on) override {
    wasPlaying = false;
    engine.reset(processSetup.sampleRate > 0 ? processSetup.sampleRate : 44100);
    return AudioEffect::setActive(on);
  }
  tresult PLUGIN_API setProcessing(TBool on) override {
    if (!on) {
      engine.reset(processSetup.sampleRate > 0 ? processSetup.sampleRate : 44100);
      wasPlaying = false;
    }
    return kResultOk;
  }
  tresult PLUGIN_API setState(IBStream *s) override {
    xs::State v;
    if (!load(s, v))
      return kResultFalse;
    states.restore(v);
    return kResultOk;
  }
  tresult PLUGIN_API getState(IBStream *s) override {
    if (!s)
      return kInvalidArgument;
    auto b = xs::encode(states.snapshot());
    int32 n = 0;
    return s->write(b.data(), int32(b.size()), &n) == kResultOk && n == b.size() ? kResultOk
                                                                                 : kResultFalse;
  }
  tresult PLUGIN_API process(ProcessData &data) override {
    DenormalGuard denormals;
    xs::StateMessage restored;
    if(states.audioRestore(restored)) {
      for(int i=0;i<xs::Count;++i)engine.set(i,restored.value[i]);
      stateGeneration=restored.generation;
    }
    if(data.processContext) {
      const auto &c=*data.processContext;
      bool playing=(c.state&ProcessContext::kPlaying)!=0;
      if(wasPlaying && !playing)engine.allOff();
      wasPlaying=playing;
      engine.transport(c.tempo,(c.state&ProcessContext::kTempoValid)!=0,
        c.projectTimeMusic,(c.state&ProcessContext::kProjectTimeMusicValid)!=0,playing);
    } else engine.transport(0,false,0,false,false);
    struct Lane {
      IParamValueQueue *queue=nullptr;
      int id=0,index=0,count=0,offset=0,previousOffset=-1;
      double value=0,previousValue=0;
      bool available=false,continuous=false;
      void advance() {
        available=index<count && queue->getPoint(index,offset,value)==kResultOk;
        ++index;
      }
    };
    std::array<Lane,HostParameterCount> lanes{};
    int active=0;
    if(data.inputParameterChanges)
      for(int q=0;q<data.inputParameterChanges->getParameterCount() && active<HostParameterCount;++q) {
        auto *queue=data.inputParameterChanges->getParameterData(q);
        if(!queue || xs::parameterIndex(queue->getParameterId())<0)continue;
        auto &lane=lanes[active++]; lane.queue=queue; lane.id=xs::parameterIndex(queue->getParameterId());
        lane.count=queue->getPointCount(); lane.previousValue=engine.state[lane.id];
        lane.continuous=xs::stepsFor(lane.id)==0 && lane.id!=AllNotesOff && lane.id!=AllSoundsOff;
        lane.advance();
      }
    int eventIndex=0,eventCount=data.inputEvents?data.inputEvents->getEventCount():0;
    Event event{};
    bool eventReady=eventCount && data.inputEvents->getEvent(0,event)==kResultOk;
    auto apply=[&](int sample) {
      for(int q=0;q<active;++q) {
        auto &lane=lanes[q];
        while(lane.available && lane.offset<=sample) {
          if(lane.id==AllNotesOff)engine.allOff();
          else if(lane.id==AllSoundsOff)engine.reset(processSetup.sampleRate);
          else engine.set(lane.id,lane.value);
          lane.previousValue=lane.value;lane.previousOffset=lane.offset;lane.advance();
        }
        if(lane.available && lane.continuous && lane.offset>lane.previousOffset && xs::valid(lane.value)) {
          double t=double(sample-lane.previousOffset)/(lane.offset-lane.previousOffset);
          engine.set(lane.id,lane.previousValue+(lane.value-lane.previousValue)*std::clamp(t,0.,1.));
        }
      }
      while(eventReady && event.sampleOffset<=sample) {
        if(event.type==Event::kNoteOnEvent)engine.note(event.noteOn.pitch,event.noteOn.velocity>0,event.noteOn.velocity,event.noteOn.noteId);
        else if(event.type==Event::kNoteOffEvent)engine.note(event.noteOff.pitch,false,0,event.noteOff.noteId);
        ++eventIndex;
        eventReady=eventIndex<eventCount && data.inputEvents->getEvent(eventIndex,event)==kResultOk;
      }
    };
    if(data.numSamples==0){apply(0);states.audioPublish(engine.state,stateGeneration);return kResultOk;}
    bool silent=true;
    for(int i=0;i<data.numSamples;++i) {
      apply(i); float y=engine.next(); silent&=y==0;
      if(data.numOutputs>0 && data.outputs)
        for(int c=0;c<data.outputs[0].numChannels;++c) {
          if(data.symbolicSampleSize==kSample32) { if(data.outputs[0].channelBuffers32[c])data.outputs[0].channelBuffers32[c][i]=y; }
          else if(data.outputs[0].channelBuffers64[c])data.outputs[0].channelBuffers64[c][i]=y;
        }
    }
    if(data.numOutputs>0 && data.outputs)data.outputs[0].silenceFlags=silent?((uint64(1)<<data.outputs[0].numChannels)-1):0;
    states.audioPublish(engine.state,stateGeneration);
    return kResultOk;
  }
};
class Controller;
class View : public CPluginView, public IPlugViewContentScaleSupport {
  Controller *controller;
  xs::Editor editor;
  double dpi = 1;
  bool hostScale = false, requestingSize = false;
  static double windowDpi(HWND parent) {
    using Fn = UINT(WINAPI *)(HWND);
    auto get = reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    auto value = get ? get(parent) : 96;
    return std::clamp(double(value ? value : 96) / 96., .5, 4.);
  }

public:
  View(Controller *c);
  ~View() override;
  tresult requestSize();
  tresult PLUGIN_API isPlatformTypeSupported(FIDString type) override {
    return type && std::strcmp(type, kPlatformTypeHWND) == 0 ? kResultTrue : kResultFalse;
  }
  tresult PLUGIN_API attached(void *parent, FIDString type) override {
    if (isPlatformTypeSupported(type) != kResultTrue || !parent || systemWindow)
      return kResultFalse;
    if (!hostScale) { dpi = windowDpi(HWND(parent)); requestSize(); }
    auto r = CPluginView::attached(parent, type);
    if (r != kResultOk)
      return r;
    if (!editor.open(HWND(parent), rect.getWidth(), rect.getHeight())) {
      CPluginView::removed();
      return kResultFalse;
    }
    return r;
  }
  tresult PLUGIN_API removed() override {
    editor.close();
    return CPluginView::removed();
  }
  tresult PLUGIN_API onSize(ViewRect *r) override;
  tresult PLUGIN_API canResize() override { return kResultTrue; }
  tresult PLUGIN_API checkSizeConstraint(ViewRect *r) override {
    if (!r) return kInvalidArgument;
    double width = r->getWidth();
    if (std::abs(r->getHeight() - rect.getHeight()) * double(xs::NativeWidth) / xs::NativeHeight >
        std::abs(r->getWidth() - rect.getWidth())) width = r->getHeight() * double(xs::NativeWidth) / xs::NativeHeight;
    const int w = int(std::round(std::clamp(width, 720. * dpi, std::min(1920. * dpi, 3000.))));
    r->right = r->left + w;
    r->bottom = r->top + int(std::round(w * double(xs::NativeHeight) / xs::NativeWidth));
    return kResultTrue;
  }
  tresult PLUGIN_API setContentScaleFactor(ScaleFactor factor) override {
    if (!std::isfinite(factor) || factor < .5 || factor > 4)
      return kInvalidArgument;
    const auto previous = dpi;
    dpi = factor;
    auto result = requestSize();
    if (result != kResultOk) dpi = previous;
    else hostScale = true;
    return result;
  }
  OBJ_METHODS(View, CPluginView)
  DEFINE_INTERFACES DEF_INTERFACE(IPlugViewContentScaleSupport) END_DEFINE_INTERFACES(CPluginView)
      REFCOUNT_METHODS(CPluginView)
};
class Controller : public EditControllerEx1, public IMidiMapping {
public:
  double editorWidth = xs::NativeWidth; // Logical pixels; monitor DPI is never serialized.
  std::vector<View *> views;
  tresult PLUGIN_API getState(IBStream *stream) override {
    if (!stream) return kInvalidArgument;
    std::array<uint8_t, 16> bytes{'X','S','E','U',1,0,0,0};
    uint64_t value; std::memcpy(&value, &editorWidth, 8);
    for (int i = 0; i < 8; ++i) bytes[8+i] = uint8_t(value >> (i*8));
    int32 written = 0;
    return stream->write(bytes.data(), int32(bytes.size()), &written) == kResultOk && written == bytes.size() ? kResultOk : kResultFalse;
  }
  tresult PLUGIN_API setState(IBStream *stream) override {
    if (!stream) return kInvalidArgument;
    std::array<uint8_t, 16> bytes{}; int32 count = 0;
    auto result = stream->read(bytes.data(), int32(bytes.size()), &count);
    if (count == 0) return kResultOk; // Earlier versions had no controller state.
    if (result != kResultOk || count != bytes.size() || std::memcmp(bytes.data(), "XSEU\1\0\0\0", 8)) return kResultFalse;
    uint64_t value = 0; for (int i = 0; i < 8; ++i) value |= uint64_t(bytes[8+i]) << (i*8);
    double width; std::memcpy(&width, &value, 8);
    if (!std::isfinite(width) || width < 400 || width > 1920) return kResultFalse;
    editorWidth = width <= 625 ? xs::NativeWidth : width;
    for (auto view : views) view->requestSize();
    return kResultOk;
  }
  tresult PLUGIN_API setParamNormalized(ParamID id, ParamValue value) override {
    int index=xs::parameterIndex(id);
    if(index<0 || !xs::valid(value))return kInvalidArgument;
    return EditControllerEx1::setParamNormalized(id,xs::stepped(index,value));
  }
  static FUnknown *create(void *) { return static_cast<IEditController *>(new Controller); }
  tresult PLUGIN_API initialize(FUnknown *c) override {
    auto r = EditControllerEx1::initialize(c);
    if (r != kResultOk)
      return r;
    auto d = xs::defaults();
    const char *sections[24] = {"VCO1",      "VCO1",      "VCO2",      "VCO2",       "LFO",
                                "LFO",       "LFO",       "MIXER",     "PORTAMENTO", "EG1 (VCA)",
                                "EG1 (VCA)", "EG1 (VCA)", "EG1 (VCA)", "EG1 (VCA)",  "EG1 (VCA)",
                                "EG2",       "EG2",       "EG2",       "EG2",        "EG2",
                                "EG2",       "VCF",       "VCF",       "VOLUME"};
    for (int i = 0; i < xs::LegacyCount; ++i) {
      std::string name;
      int steps = 0;
      if (i < 24)
        name = std::string(sections[i]) + " " + xs::specs[i].name;
      else if (i < 27) {
        name = i == 24 ? "VCO1 waveform" : i == 25 ? "VCO2 waveform" : "LFO waveform";
        steps = 5;
      } else {
        name = i == 27   ? "oscillator sync"
               : i == 28 ? "12-24 db/oct"
               : i == 29 ? "POWER"
               : i == 30 ? "pitch bend"
                         : "modulation";
        steps = i < 30 ? 1 : 0;
      }
      String128 title{};
      UString(title, 128).fromAscii(name.c_str());
      parameters.addParameter(title, nullptr, steps, d[i], ParameterInfo::kCanAutomate, xs::hostId(i));
    }
    parameters.addParameter(STR16("MIDI All Notes Off"), nullptr, 0, 0, ParameterInfo::kIsHidden, xs::hostId(AllNotesOff));
    parameters.addParameter(STR16("MIDI All Sounds Off"), nullptr, 0, 0, ParameterInfo::kIsHidden, xs::hostId(AllSoundsOff));
    for(int i=34;i<xs::Count;++i) {
      char label[96]{};
      if(i<xs::MatrixFirst)std::snprintf(label,sizeof label,"%s",xs::extraSpecs[i-34].name);
      else std::snprintf(label,sizeof label,"Matrix %d %s",(i-xs::MatrixFirst)/3+1,(i-xs::MatrixFirst)%3==0?"Source":(i-xs::MatrixFirst)%3==1?"Destination":"Amount");
      String128 title{};UString(title,128).fromAscii(label);
      parameters.addParameter(title,nullptr,xs::stepsFor(i),d[i],i==xs::Pressure?ParameterInfo::kIsHidden:ParameterInfo::kCanAutomate,xs::hostId(i));
    }
    return kResultOk;
  }
  IPlugView *PLUGIN_API createView(FIDString name) override {
    return name && std::strcmp(name, ViewType::kEditor) == 0 ? new View(this) : nullptr;
  }
  tresult PLUGIN_API setComponentState(IBStream *s) override {
    xs::State v;
    if (!load(s, v))
      return kResultFalse;
    for (int i = 0; i < xs::Count; ++i)
      setParamNormalized(xs::hostId(i), v[i]);
    return kResultOk;
  }
  tresult PLUGIN_API getMidiControllerAssignment(int32 bus, int16 channel, CtrlNumber cc,
                                                 ParamID &id) override {
    if (bus != 0 || channel < 0 || channel > 15)
      return kResultFalse;
    if (cc == kPitchBend) {
      id = xs::hostId(30);
      return kResultTrue;
    }
    if (cc == kCtrlModWheel) {
      id = xs::hostId(31);
      return kResultTrue;
    }
    if (cc == kAfterTouch) { id=xs::hostId(xs::Pressure); return kResultTrue; }
    if (cc == kCtrlAllNotesOff || cc == kCtrlAllSoundsOff) {
      id = xs::hostId(cc == kCtrlAllNotesOff ? AllNotesOff : AllSoundsOff);
      return kResultTrue;
    }
    return kResultFalse;
  }
  tresult PLUGIN_API getParamStringByValue(ParamID hostId, ParamValue value, String128 text) override {
    int id=xs::parameterIndex(hostId);if(id<0)return kInvalidArgument;
    if (id >= HostParameterCount || !xs::valid(value)) return kInvalidArgument;
    value = xs::stepped(id, value);
    char b[64]{};
    if (id >= 34) xs::extraText(id,value,b,sizeof b);
    else if (id < 24)
      std::snprintf(b, sizeof b, "%.5g", xs::physical(id, value));
    else if (id < 27) {
      const char *w[] = {"sine", "triangle", "up sawtooth", "down sawtooth", "square", "pulse"};
      std::snprintf(b, sizeof b, "%s", w[std::clamp(int(std::round(value * 5)), 0, 5)]);
    } else if (id == 28)
      std::snprintf(b, sizeof b, "%s", value >= .5 ? "24 dB/oct" : "12 dB/oct");
    else if (id == 27 || id == 29)
      std::snprintf(b, sizeof b, "%s", value >= .5 ? "On" : "Off");
    else
      std::snprintf(b, sizeof b, "%.4g", value);
    UString(text, 128).fromAscii(b);
    return kResultOk;
  }
  tresult PLUGIN_API getParamValueByString(ParamID hostId, TChar *text, ParamValue &value) override {
    int id=xs::parameterIndex(hostId);if(id<0)return kInvalidArgument;
    if (!text || id >= HostParameterCount)
      return kInvalidArgument;
    char b[128]{};
    UString(text, 128).toAscii(b, 128);
    if(id>=34 && xs::stepsFor(id)>0) {
      for(int i=0;i<=xs::stepsFor(id);++i) { char choice[96]{};xs::extraText(id,double(i)/xs::stepsFor(id),choice,sizeof choice);
        if(!std::strcmp(b,choice)){value=double(i)/xs::stepsFor(id);return kResultOk;} }
      return kResultFalse;
    }
    if (id >= 27 && id <= 29) {
      if (std::strcmp(b, id == 28 ? "24 dB/oct" : "On") == 0) { value = 1; return kResultOk; }
      if (std::strcmp(b, id == 28 ? "12 dB/oct" : "Off") == 0) { value = 0; return kResultOk; }
    }
    if (id >= 24 && id < 27) {
      const char *w[] = {"sine", "triangle", "up sawtooth", "down sawtooth", "square", "pulse"};
      for (int i = 0; i < 6; ++i)
        if (std::strcmp(b, w[i]) == 0) {
          value = i / 5.;
          return kResultOk;
        }
      return kResultFalse;
    }
    char *end{};
    double v = std::strtod(b, &end);
    if(id==xs::MasterTune && end && std::strcmp(end," ct")==0)end+=3;
    if (end == b || *end || !std::isfinite(v))
      return kResultFalse;
    if(id==xs::MasterTune || (id>=xs::MatrixFirst && (id-xs::MatrixFirst)%3==2)) {
      value=std::clamp(v/200+.5,0.,1.);return kResultOk;
    }
    value = xs::stepped(id, id < 24 ? xs::normalized(id, v) : std::clamp(v, 0., 1.));
    return xs::valid(value) ? kResultOk : kResultFalse;
  }
  ParamValue PLUGIN_API normalizedParamToPlain(ParamID hostId, ParamValue v) override {
    int id=xs::parameterIndex(hostId);if(id<0)return 0;
    if(id>=34 && xs::stepsFor(id))return xs::discrete(id,v);
    if(id==xs::MasterTune)return (v*2-1)*100;
    if(id>=xs::MatrixFirst)return (v*2-1)*100;
    if (id >= 24 && id < 27) return std::round(std::clamp(v, 0., 1.) * 5.);
    if (id >= 27 && id < 30) return xs::stepped(id, v);
    return id < 24 ? xs::physical(id, v) : EditControllerEx1::normalizedParamToPlain(hostId, v);
  }
  ParamValue PLUGIN_API plainParamToNormalized(ParamID hostId, ParamValue v) override {
    int id=xs::parameterIndex(hostId);if(id<0)return 0;
    if(id>=34 && xs::stepsFor(id))return std::round(std::clamp(v,0.,double(xs::stepsFor(id))))/xs::stepsFor(id);
    if(id==xs::MasterTune || id>=xs::MatrixFirst)return std::clamp(v/200+.5,0.,1.);
    if (id >= 24 && id < 27) return std::round(std::clamp(v, 0., 5.)) / 5.;
    if (id >= 27 && id < 30) return xs::stepped(id, v);
    return id < 24 ? xs::normalized(id, v) : EditControllerEx1::plainParamToNormalized(hostId, v);
  }
  OBJ_METHODS(Controller, EditControllerEx1)
  DEFINE_INTERFACES DEF_INTERFACE(IMidiMapping) END_DEFINE_INTERFACES(EditControllerEx1)
      REFCOUNT_METHODS(EditControllerEx1)
};
View::View(Controller *c) : controller(c) {
  controller->addRef();
  controller->views.push_back(this);
  rect = ViewRect(0, 0, int32(std::round(c->editorWidth)), int32(std::round(c->editorWidth * double(xs::NativeHeight) / xs::NativeWidth)));
  editor.dpiChanged = [this] {
    if (!hostScale && systemWindow) { dpi = windowDpi(HWND(systemWindow)); requestSize(); }
  };
  editor.read = [this] {
    xs::State s;
    for (int i = 0; i < xs::Count; ++i)
      s[i] = controller->getParamNormalized(xs::hostId(i));
    return s;
  };
  editor.begin = [this](int id) { controller->beginEdit(xs::hostId(id)); };
  editor.end = [this](int id) { controller->endEdit(xs::hostId(id)); };
  editor.change = [this](int id, double v) {
    controller->setParamNormalized(xs::hostId(id), v);
    controller->performEdit(xs::hostId(id), v);
  };
}
View::~View() {
  editor.close();
  auto &views = controller->views;
  views.erase(std::remove(views.begin(), views.end(), this), views.end());
  controller->release();
}
tresult View::requestSize() {
  const auto width = int32(std::round(std::min(controller->editorWidth * dpi, 3000.)));
  ViewRect desired(0, 0, width, int32(std::round(width * double(xs::NativeHeight) / xs::NativeWidth)));
  if (desired.getWidth() == rect.getWidth() && desired.getHeight() == rect.getHeight()) return kResultOk;
  requestingSize = true;
  auto result = plugFrame ? plugFrame->resizeView(this, &desired) : onSize(&desired);
  requestingSize = false;
  return result;
}
tresult View::onSize(ViewRect *r) {
  if (!r || r->getWidth() < 1 || r->getWidth() > 3000 || r->getHeight() < 1 ||
      std::abs(r->getHeight() - r->getWidth() * double(xs::NativeHeight) / xs::NativeWidth) > 1.) return kInvalidArgument;
  if (!requestingSize) controller->editorWidth = std::clamp(r->getWidth() / dpi, 720., 1920.);
  editor.resize(r->getWidth(), r->getHeight());
  return CPluginView::onSize(r);
}
} // namespace
BEGIN_FACTORY_DEF(XSYNTH_VENDOR_NAME, "", "")
DEF_CLASS2(INLINE_UID_FROM_FUID(ProcessorID), PClassInfo::kManyInstances, kVstAudioEffectClass,
           XSYNTH_PRODUCT_NAME, 0, "Instrument|Synth", XSYNTH_VERSION, kVstVersionString, Processor::create)
DEF_CLASS2(INLINE_UID_FROM_FUID(ControllerID), PClassInfo::kManyInstances,
           kVstComponentControllerClass, XSYNTH_PRODUCT_NAME " Controller", 0, "", XSYNTH_VERSION, kVstVersionString,
           Controller::create)
END_FACTORY
