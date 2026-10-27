// GPL-2.0-or-later. Original Xsynth (C) 1999 S. J. Brookes.
#include "Core.h"
#include "NewInstanceDefaults.h"
#include "OutputProtect.h"
#include "ProtectState.h"
#include <atomic>
#include "Editor.h"
#include "Version.h"
#include "StateExchange.h"
#include "DocumentExchange.h"
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
#include <future>
using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
constexpr ParamID AllNotesOff = xs::AllNotesOff, AllSoundsOff = xs::AllSoundsOff;
constexpr int HostParameterCount = xs::Count;
static_assert(std::atomic<bool>::is_always_lock_free,"PROTECT flag must remain lock-free");
static_assert(std::atomic<double>::is_always_lock_free,"GUI clock telemetry must remain lock-free");
const FUID ProcessorID(0x7923268C, 0x1C9D43DE, 0xAA65DAAC, 0x78C5232D);
const FUID ControllerID(0x0C0B572A, 0x7CD34DC5, 0x95F21C41, 0x22650E26);
bool load(IBStream *stream,xs::State&state,xs::table::Document* assets=nullptr,bool* protect=nullptr){try{
  std::vector<uint8_t> bytes(8);int32 n=0;
  if(!stream||stream->read(bytes.data(),8,&n)!=kResultOk||n!=8)return false;
  size_t total=0;
  if(!std::memcmp(bytes.data(),"XSED\7\0\0\0",8)){
    bytes.resize(32);if(stream->read(bytes.data()+8,24,&n)!=kResultOk||n!=24)return false;
    total=xs::documentState::u32(bytes.data()+12);if(total<32||total>xs::table::MaxStateBytes)return false;
  }else{int count=xs::stateValueCount(bytes.data());if(!count)return false;total=8+size_t(count)*8;}
  size_t header=bytes.size();bytes.resize(total);
  if(stream->read(bytes.data()+header,int32(total-header),&n)!=kResultOk||n!=total-header)return false;
  xs::table::Document doc;auto prepared=std::async(std::launch::async,[&]{return xs::decodeDocument(bytes.data(),bytes.size(),state,doc);});if(!prepared.get())return false;
  bool enabled=false;if(!xs::protect::readHost(bytes,enabled))return false;
  if(protect)*protect=enabled;
  if(assets)*assets=std::move(doc);return true;
}catch(...){return false;}
}
struct DenormalGuard {
 unsigned previous=_mm_getcsr();
 DenormalGuard(){_mm_setcsr(previous|0x8040);}
 ~DenormalGuard(){_mm_setcsr(previous);}
};
class Processor : public AudioEffect {
  xs::Engine engine;
  xs::protect::Engine protection;
  std::atomic<bool> protectEnabled{true}; // New instances ON; old host states without metadata restore OFF.
  const xs::table::Bank* factory=&xs::table::factoryBank();
  std::atomic<double> editorTempo{120},editorRate{44100};
  bool wasPlaying = false;
  xs::DocumentExchange states;
  std::mutex preparationOnly;
  uint64_t stateGeneration=0;

public:
  Processor() { setControllerClass(ControllerID);engine.state=xs::newInstanceState();states.restore(engine.state);engine.setAssets(&xs::table::factoryBank());protection.prepare(44100,1,20,-1,true); }
  uint32 PLUGIN_API getLatencySamples() override {return uint32(protection.latency);}
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
    engine.reset(s.sampleRate);editorTempo.store(120,std::memory_order_relaxed);editorRate.store(s.sampleRate,std::memory_order_relaxed);
    // Bounded recovery avoids a long exponential attenuation tail after a rare peak.
    protection.prepare(s.sampleRate,1,20,-1,true);
    return AudioEffect::setupProcessing(s);
  }
  tresult PLUGIN_API setActive(TBool on) override {
    wasPlaying = false;
    protection.reset();
    engine.reset(processSetup.sampleRate > 0 ? processSetup.sampleRate : 44100);
    return AudioEffect::setActive(on);
  }
  tresult PLUGIN_API setProcessing(TBool on) override {
    if (!on) {
      protection.reset();
      engine.reset(processSetup.sampleRate > 0 ? processSetup.sampleRate : 44100);
      wasPlaying = false;
    }
    return kResultOk;
  }
  tresult PLUGIN_API notify(IMessage *message) override {
    // Requested by the editor outside process(); message allocation never happens in audio rendering.
    if(message&&message->getMessageID()&&!std::strcmp(message->getMessageID(),"XsynthClockRequest")){
      auto*reply=allocateMessage();if(!reply)return kResultFalse;reply->setMessageID("XsynthClockReply");reply->getAttributes()->setFloat("tempo",editorTempo.load(std::memory_order_relaxed));reply->getAttributes()->setFloat("rate",editorRate.load(std::memory_order_relaxed));auto result=sendMessage(reply);reply->release();return result;
    }
    if(message&&message->getMessageID()&&!std::strcmp(message->getMessageID(),"XsynthProtect")){
      int64 enabled=0;if(message->getAttributes()->getInt("enabled",enabled)!=kResultOk||(enabled!=0&&enabled!=1))return kInvalidArgument;
      protectEnabled.store(enabled!=0,std::memory_order_relaxed);return kResultOk;
    }
    if(message && message->getMessageID() && !std::strcmp(message->getMessageID(),"XsynthPatch")){
      std::lock_guard<std::mutex> preparation(preparationOnly);
      const void *data=nullptr;uint32 bytes=0;xs::State restored;xs::table::Document doc;
      if(message->getAttributes()->getBinary("state",data,bytes)!=kResultOk)return kInvalidArgument;
      auto prepared=std::async(std::launch::async,[&]{return xs::decodeDocument(static_cast<const uint8_t*>(data),bytes,restored,doc);});if(!prepared.get())return kInvalidArgument;
      return states.restore(restored,doc,true)?kResultOk:kResultFalse;
    }
    return AudioEffect::notify(message);
  }
  tresult PLUGIN_API setState(IBStream *s) override {
    std::lock_guard<std::mutex> preparation(preparationOnly);
    xs::State v;xs::table::Document doc;
    bool enabled=false;
    if (!load(s, v,&doc,&enabled))
      return kResultFalse;
    if(!states.restore(v,doc))return kResultFalse;
    protectEnabled.store(enabled,std::memory_order_relaxed);return kResultOk;
  }
  tresult PLUGIN_API getState(IBStream *s) override {
    if (!s)
      return kInvalidArgument;
    auto snapshot=states.snapshot();auto b=xs::protect::encodeHost(snapshot.first,snapshot.second,protectEnabled.load(std::memory_order_relaxed));
    int32 n = 0;
    return s->write(b.data(), int32(b.size()), &n) == kResultOk && n == b.size() ? kResultOk
                                                                                 : kResultFalse;
  }
  tresult PLUGIN_API process(ProcessData &data) override {
    DenormalGuard denormals;
    bool numericChanged=false;
    xs::StateMessage restored;
    const xs::table::Document* restoredAssets=nullptr;int assetOwner=-1;
    if(states.audioRestore(restored,restoredAssets,assetOwner)) {
      engine.setAssets(factory,restoredAssets,assetOwner);
      for(int i=0;i<xs::Count;++i)if(!restored.preservePerformance || (i!=30&&i!=31&&i!=xs::AllNotesOff&&i!=xs::AllSoundsOff&&i!=xs::Pressure&&i!=xs::Sustain&&i!=xs::Sostenuto))engine.set(i,restored.value[i]);
      stateGeneration=restored.generation;numericChanged=true;
    }
    if(data.processContext) {
      const auto &c=*data.processContext;
      bool playing=(c.state&ProcessContext::kPlaying)!=0;
      if(wasPlaying && !playing){engine.allOff();numericChanged=true;}
      wasPlaying=playing;
      if((c.state&ProcessContext::kTempoValid)&&std::isfinite(c.tempo)&&c.tempo>0)editorTempo.store(c.tempo,std::memory_order_relaxed);
      engine.transport(c.tempo,(c.state&ProcessContext::kTempoValid)!=0,
        c.projectTimeMusic,(c.state&ProcessContext::kProjectTimeMusicValid)!=0,playing);
    } else engine.transport(0,false,0,false,false);
    struct Lane {
      IParamValueQueue *queue;
      int id,index,count,offset,previousOffset,priority;
      double value,previousValue;
      bool available,continuous;
      void advance() {
        available=index<count && queue->getPoint(index,offset,value)==kResultOk;
        ++index;
      }
    };
    std::array<Lane,HostParameterCount> lanes;
    int active=0;
    if(data.inputParameterChanges)
      for(int q=0;q<data.inputParameterChanges->getParameterCount() && active<HostParameterCount;++q) {
        auto *queue=data.inputParameterChanges->getParameterData(q);
        if(!queue || xs::parameterIndex(queue->getParameterId())<0)continue;
        auto &lane=lanes[active++];lane={};lane.previousOffset=-1; lane.queue=queue; lane.id=xs::parameterIndex(queue->getParameterId());
        lane.count=queue->getPointCount(); lane.previousValue=engine.state[lane.id];
        lane.priority=lane.id==xs::Voices||xs::legacyDestination(lane.id)||xs::matrixSource(lane.id)?2:xs::expandedDestination(lane.id)?1:0;
        lane.continuous=xs::stepsFor(lane.id)==0 && lane.id!=AllNotesOff && lane.id!=AllSoundsOff;
        lane.advance();
      }
    int eventIndex=0,eventCount=data.inputEvents?data.inputEvents->getEventCount():0;
    Event event{};
    bool eventReady=eventCount && data.inputEvents->getEvent(0,event)==kResultOk;
    auto apply=[&](int sample) {
      for(int pass=0;pass<3;++pass)for(int q=0;q<active;++q) {
        auto &lane=lanes[q];
        if(lane.priority!=pass||!lane.available)continue;
        while(lane.available && lane.offset<=sample) {
          if(lane.id==AllNotesOff){engine.allOff();numericChanged=true;}
          else if(lane.id==AllSoundsOff){engine.reset(processSetup.sampleRate);protection.reset();numericChanged=true;}
          else {engine.set(lane.id,lane.value);numericChanged=true;}
          lane.previousValue=lane.value;lane.previousOffset=lane.offset;lane.advance();
        }
        if(lane.available && lane.continuous && lane.offset>lane.previousOffset && xs::valid(lane.value)) {
          double t=double(sample-lane.previousOffset)/(lane.offset-lane.previousOffset);
          engine.set(lane.id,lane.previousValue+(lane.value-lane.previousValue)*std::clamp(t,0.,1.));numericChanged=true;
        }
      }
      while(eventReady && event.sampleOffset<=sample) {
        if(event.type==Event::kNoteOnEvent)engine.note(event.noteOn.pitch,event.noteOn.velocity>0,event.noteOn.velocity,event.noteOn.noteId);
        else if(event.type==Event::kNoteOffEvent)engine.note(event.noteOff.pitch,false,0,event.noteOff.noteId);
        ++eventIndex;
        eventReady=eventIndex<eventCount && data.inputEvents->getEvent(eventIndex,event)==kResultOk;
      }
    };
    if(data.numSamples==0){apply(0);states.audioPublish(engine.state,stateGeneration,engine.assetOwners(),numericChanged);return kResultOk;}
    bool silent=true;
    const bool protecting=protectEnabled.load(std::memory_order_relaxed);
    for(int i=0;i<data.numSamples;++i) {
      if(active||eventReady)apply(i); engine.next();float left=engine.outputLeft(),right=engine.outputRight();protection.sample(left,right,left,right,protecting);silent&=left==0&&right==0;
      if(data.numOutputs>0 && data.outputs)
        for(int c=0;c<data.outputs[0].numChannels;++c) {
          float y=data.outputs[0].numChannels==1?(left+right)*.5f:c==0?left:right;
          if(data.symbolicSampleSize==kSample32) { if(data.outputs[0].channelBuffers32[c])data.outputs[0].channelBuffers32[c][i]=y; }
          else if(data.outputs[0].channelBuffers64[c])data.outputs[0].channelBuffers64[c][i]=y;
        }
    }
    if(data.numOutputs>0 && data.outputs)data.outputs[0].silenceFlags=silent?((uint64(1)<<data.outputs[0].numChannels)-1):0;
    states.audioPublish(engine.state,stateGeneration,engine.assetOwners(),numericChanged);
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
  std::atomic<double> latestTempo{120},latestRate{48000};ULONGLONG clockRequestAt=0;
  void requestClock(){auto now=GetTickCount64();if(now-clockRequestAt<250)return;clockRequestAt=now;auto*message=allocateMessage();if(!message)return;message->setMessageID("XsynthClockRequest");sendMessage(message);message->release();}
  tresult PLUGIN_API notify(IMessage*message)override{if(message&&message->getMessageID()&&!std::strcmp(message->getMessageID(),"XsynthClockReply")){double tempo=0,rate=0;if(message->getAttributes()->getFloat("tempo",tempo)!=kResultOk||message->getAttributes()->getFloat("rate",rate)!=kResultOk||!std::isfinite(tempo)||tempo<=0||!std::isfinite(rate)||rate<=0)return kInvalidArgument;latestTempo.store(tempo,std::memory_order_relaxed);latestRate.store(rate,std::memory_order_relaxed);return kResultOk;}return EditControllerEx1::notify(message);}
  int lastFilter=0,lastCycle=0;
  bool protectEnabled=true;
  bool setProtect(bool enabled){
    auto* message=allocateMessage();if(!message)return false;
    message->setMessageID("XsynthProtect");message->getAttributes()->setInt("enabled",enabled?1:0);
    const bool ok=sendMessage(message)==kResultOk;message->release();
    if(ok){protectEnabled=enabled;setDirty(true);}return ok;
  }
  xs::table::Document assets;
  std::mutex assetsOnly;
  uint64_t assetsRevision=0;
  uint64_t documentRevision(){std::lock_guard<std::mutex>lock(assetsOnly);return assetsRevision;}
  xs::table::Document documentAssets(){std::lock_guard<std::mutex>lock(assetsOnly);return assets;}
  void publishAssets(const xs::table::Document&doc){std::lock_guard<std::mutex>lock(assetsOnly);assets=doc;++assetsRevision;}
  double editorWidth = xs::NativeWidth; // Logical pixels; monitor DPI is never serialized.
  std::array<int,2> lastTableSource{1,1}; // UI-only recall, independent per VCO.
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
    if(index==xs::Voices)EditControllerEx1::setParamNormalized(xs::hostId(xs::VoicesExtension),0.);
    if(index>=xs::PositionRouteFirst&&index<xs::RasterFirst)EditControllerEx1::setParamNormalized(xs::hostId(xs::BFilterRouteFirst+index-xs::PositionRouteFirst),0.);
    if(xs::matrixDestination(index)||xs::expandedDestination(index)||(index>=xs::DetuneRouteFirst&&index<xs::PreFourLfoCount)){int slot=xs::matrixDestination(index)?xs::matrixSlot(index):xs::expandedDestination(index)?index-xs::ExpandedDestinationFirst:index-xs::DetuneRouteFirst;EditControllerEx1::setParamNormalized(xs::hostId(xs::PositionRouteFirst+slot),0.);EditControllerEx1::setParamNormalized(xs::hostId(xs::BFilterRouteFirst+slot),0.);}
    if(xs::matrixSource(index))EditControllerEx1::setParamNormalized(xs::hostId(xs::SourceExtensionFirst+xs::matrixSlot(index)),0.);
    if(xs::legacyDestination(index)||xs::expandedDestination(index))EditControllerEx1::setParamNormalized(xs::hostId(xs::DetuneRouteFirst+(xs::legacyDestination(index)?(index-xs::MatrixFirst)/3:index-xs::ExpandedDestinationFirst)),0.);
    if(xs::legacyDestination(index))EditControllerEx1::setParamNormalized(xs::hostId(xs::ExpandedDestinationFirst+(index-xs::MatrixFirst)/3),0.);
    return EditControllerEx1::setParamNormalized(id,xs::stepped(index,value));
  }
  static FUnknown *create(void *) { return static_cast<IEditController *>(new Controller); }
  tresult PLUGIN_API initialize(FUnknown *c) override {
    auto r = EditControllerEx1::initialize(c);
    if (r != kResultOk)
      return r;
    auto d = xs::newInstanceState();
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
      if(i==xs::DriveExtension)std::snprintf(label,sizeof label,"Drive extension (additional %%)");
      else if(i==xs::VoicesExtension)std::snprintf(label,sizeof label,"Voice count extension");
      else if(i>=xs::PreDistortionCount){const char*names[]={"Distortion mode","Distortion drive","Distortion mix","Distortion placement"};std::snprintf(label,sizeof label,"%s",names[i-xs::PreDistortionCount]);}
      else if(i>=xs::PreDualCount){const char*names[]={"EG2 sync policy","EG2 sync rate","VCF B cutoff","VCF B resonance","VCF B keytrack","VCF B type","VCF B slope","VCF route"};if(i<xs::BFilterRouteFirst)std::snprintf(label,sizeof label,"%s",names[i-xs::PreDualCount]);else std::snprintf(label,sizeof label,"Matrix %d VCF B destination",i-xs::BFilterRouteFirst+1);}
      else if(i==xs::Sustain||i==xs::Sostenuto)std::snprintf(label,sizeof label,"MIDI %s",i==xs::Sustain?"Sustain":"Sostenuto");
      else if(i==xs::Width)std::snprintf(label,sizeof label,"Unison width");
      else if(i>=xs::RasterFirst)std::snprintf(label,sizeof label,"VCO%d RASTER32",i-xs::RasterFirst+1);
      else if(i>=xs::PositionRouteFirst)std::snprintf(label,sizeof label,"Matrix %d Position Override",i-xs::PositionRouteFirst+1);
      else if(i>=xs::OscSourceFirst){const char*names[]={"Source","Factory table","USER slot","Position","Sample read","Frame read"};std::snprintf(label,sizeof label,"VCO%d %s",1+(i-xs::OscSourceFirst)/6,names[(i-xs::OscSourceFirst)%6]);}
      else if(i<xs::MatrixFirst)std::snprintf(label,sizeof label,"%s",xs::extraSpecs[i-34].name);
      else if(i==xs::Unison)std::snprintf(label,sizeof label,"Unison");
      else if(i==xs::Detune)std::snprintf(label,sizeof label,"Unison detune");
      else if(i>=xs::ExtraLfoFirst&&i<xs::NewMatrixFirst){const char*fields[]={"Rate","Pitch Depth","Filter Depth","Wave","Clock","Phase","Polarity","Digital Wave","Division"};std::snprintf(label,sizeof label,"LFO %d %s",2+(i-xs::ExtraLfoFirst)/9,fields[(i-xs::ExtraLfoFirst)%9]);}
      else if(xs::sourceExtension(i))std::snprintf(label,sizeof label,"Matrix %d LFO Source Extension",i-xs::SourceExtensionFirst+1);
      else if(i>=xs::NewMatrixFirst&&i<xs::NewDestinationFirst)std::snprintf(label,sizeof label,"Matrix %d %s",xs::matrixSlot(i)+1,(i-xs::NewMatrixFirst)%3==0?"Source":(i-xs::NewMatrixFirst)%3==1?"Destination":"Amount");
      else if(i>=xs::DetuneRouteFirst&&i<xs::PreFourLfoCount)std::snprintf(label,sizeof label,"Matrix %d Unison Detune Route",i-xs::DetuneRouteFirst+1);
      else if(xs::expandedDestination(i))std::snprintf(label,sizeof label,"Matrix %d Expanded Destination",i-xs::ExpandedDestinationFirst+1);
      else std::snprintf(label,sizeof label,"Matrix %d %s",(i-xs::MatrixFirst)/3+1,(i-xs::MatrixFirst)%3==0?"Source":(i-xs::MatrixFirst)%3==1?"Destination":"Amount");
      String128 title{};UString(title,128).fromAscii(label);
      parameters.addParameter(title,nullptr,xs::stepsFor(i),d[i],i==xs::Pressure||i==xs::Sustain||i==xs::Sostenuto?ParameterInfo::kIsHidden:ParameterInfo::kCanAutomate,xs::hostId(i));
    }
    return kResultOk;
  }
  bool restorePatch(const xs::State &state,const xs::table::Document&doc={}){
    bool delivered=false;
    if(auto *message=allocateMessage()){
      auto bytes=xs::encodeDocument(state,doc);message->setMessageID("XsynthPatch");
      if(message->getAttributes()->setBinary("state",bytes.data(),uint32(bytes.size()))==kResultOk)delivered=sendMessage(message)==kResultOk;
      message->release();
    }
    // Legacy-only patches can still travel through ordinary parameter edits
    // when a minimal host has no component messaging connection.
    if(!delivered){for(const auto&t:doc.slots)if(t)return false;for(int slot=0;slot<6;++slot)if(state[xs::ExpandedDestinationFirst+slot]>0||state[xs::DetuneRouteFirst+slot]>0)return false;for(int slot=0;slot<xs::MatrixSlots;++slot)if(state[xs::SourceExtensionFirst+slot]>0||state[xs::BFilterRouteFirst+slot]>0)return false;}
    publishAssets(doc);for(int i=0;i<xs::Count;++i)setParamNormalized(xs::hostId(i),state[i]);
    return true;
  }
  IPlugView *PLUGIN_API createView(FIDString name) override {
    return name && std::strcmp(name, ViewType::kEditor) == 0 ? new View(this) : nullptr;
  }
  tresult PLUGIN_API setComponentState(IBStream *s) override {
    xs::State v;xs::table::Document doc;
    bool enabled=false;
    if (!load(s, v,&doc,&enabled))
      return kResultFalse;
    protectEnabled=enabled;
    publishAssets(doc);
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
    if (cc == 64 || cc == 66){id=xs::hostId(cc==64?xs::Sustain:xs::Sostenuto);return kResultTrue;}
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
    if(id>=xs::OscSourceFirst&&id<xs::PositionRouteFirst&&((id-xs::OscSourceFirst)%6==1||(id-xs::OscSourceFirst)%6==2)){char*end=nullptr;long number=std::strtol(b,&end,10);if(end==b||*end||number<0||number>xs::stepsFor(id))return kResultFalse;value=number/double(xs::stepsFor(id));return kResultOk;}
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
    if(id>=xs::ExtraLfoFirst&&id<xs::NewMatrixFirst&&!xs::stepsFor(id)){
      int base=xs::lfoBase(id);if(end==b||!std::isfinite(v))return kResultFalse;
      if(base==4&&std::strcmp(end," Hz")==0){value=xs::normalized(4,std::max(.1,v));return kResultOk;}
      if((base==5||base==6)&&std::strcmp(end,"%")==0){value=std::clamp(v/100,0.,1.);return kResultOk;}
      return kResultFalse;
    }
    if(id==xs::Width||id==xs::DriveExtension||id==xs::DistDrive||id==xs::DistMix||id==xs::oscParam(0,3)||id==xs::oscParam(1,3)){if(end==b||!std::isfinite(v)||(*end&&std::strcmp(end,"%")))return kResultFalse;value=std::clamp(v/100,0.,1.);return kResultOk;}
    if(id==xs::Detune && end && std::strcmp(end," ct")==0)end+=3;
    if(id==xs::MasterTune && end && std::strcmp(end," ct")==0)end+=3;
    if (end == b || *end || !std::isfinite(v))
      return kResultFalse;
    if(id==xs::Detune){value=std::clamp(v/50,0.,1.);return kResultOk;}
    if(id==xs::MasterTune || (id>=xs::MatrixFirst && (id-xs::MatrixFirst)%3==2)) {
      value=std::clamp(v/200+.5,0.,1.);return kResultOk;
    }
    value = xs::stepped(id, id < 24 ? xs::normalized(id, v) : std::clamp(v, 0., 1.));
    return xs::valid(value) ? kResultOk : kResultFalse;
  }
  ParamValue PLUGIN_API normalizedParamToPlain(ParamID hostId, ParamValue v) override {
    int id=xs::parameterIndex(hostId);if(id<0)return 0;
    if(id>=34 && xs::stepsFor(id))return xs::discrete(id,v);
    if(id>=xs::ExtraLfoFirst&&id<xs::NewMatrixFirst)return xs::lfoBase(id)==4?xs::physical(4,v):v*100;
    if(id==xs::Width||id==xs::DriveExtension||id==xs::DistDrive||id==xs::DistMix||id==xs::oscParam(0,3)||id==xs::oscParam(1,3))return v*100;
    if(id==xs::Detune)return v*50;
    if(id==xs::MasterTune)return (v*2-1)*100;
    if(id>=xs::MatrixFirst)return (v*2-1)*100;
    if (id >= 24 && id < 27) return std::round(std::clamp(v, 0., 1.) * 5.);
    if (id >= 27 && id < 30) return xs::stepped(id, v);
    return id < 24 ? xs::physical(id, v) : EditControllerEx1::normalizedParamToPlain(hostId, v);
  }
  ParamValue PLUGIN_API plainParamToNormalized(ParamID hostId, ParamValue v) override {
    int id=xs::parameterIndex(hostId);if(id<0)return 0;
    if(id>=34 && xs::stepsFor(id))return std::round(std::clamp(v,0.,double(xs::stepsFor(id))))/xs::stepsFor(id);
    if(id>=xs::ExtraLfoFirst&&id<xs::NewMatrixFirst)return xs::lfoBase(id)==4?xs::normalized(4,std::max(.1,v)):std::clamp(v/100,0.,1.);
    if(id==xs::Width||id==xs::DriveExtension||id==xs::DistDrive||id==xs::DistMix||id==xs::oscParam(0,3)||id==xs::oscParam(1,3))return std::clamp(v/100,0.,1.);
    if(id==xs::Detune)return std::clamp(v/50,0.,1.);
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
  editor.readProtect=[this]{return controller->protectEnabled;};
  editor.changeProtect=[this](bool enabled){return controller->setProtect(enabled);};
  editor.lastTableSource=controller->lastTableSource;editor.selectedFilter=controller->lastFilter;editor.lastEg2Cycle=controller->lastCycle;
  editor.readTempo=[this]{controller->requestClock();return controller->latestTempo.load(std::memory_order_relaxed);};editor.readSampleRate=[this]{return controller->latestRate.load(std::memory_order_relaxed);};
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
  editor.readAssetsRevision=[this]{return controller->documentRevision();};
  editor.readAssets=[this]{return controller->documentAssets();};
  editor.restoreDocument=[this](const xs::State&s,const xs::table::Document&doc){return controller->restorePatch(s,doc);};
  editor.restorePatch = [this](const xs::State &state){return controller->restorePatch(state);};
  editor.begin = [this](int id) { controller->beginEdit(xs::hostId(id)); };
  editor.end = [this](int id) { controller->endEdit(xs::hostId(id)); };
  editor.change = [this](int id, double v) {
    controller->setParamNormalized(xs::hostId(id), v);
    controller->performEdit(xs::hostId(id), v);
  };
}
View::~View() {
  controller->lastTableSource=editor.lastTableSource;controller->lastFilter=editor.selectedFilter;controller->lastCycle=editor.lastEg2Cycle;
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


