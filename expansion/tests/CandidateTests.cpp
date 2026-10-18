// GPL-2.0-or-later. Purpose-built Windows VST3 technical validation host.
// All product behavior is exercised through the freshly linked DLL's VST3 ABI.
#define NOMINMAX
#include "Core.h"
#include "DssiImport.h"
#include "DssiDialog.h"
#include "PresetBrowser.h"
#include "UiGeometry.h"
#include "ContextRegions.h"
#include "ThemePreference.h"
#include "ProductIdentity.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include <windows.h>
#include <commdlg.h>
#include <dlgs.h>
#include <gdiplus.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace Steinberg;
using namespace Steinberg::Vst;
namespace fs=std::filesystem;
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
void closeValue(double a,double b,const char*m){check(std::abs(a-b)<1e-9,m);}
std::vector<uint8_t> read(const fs::path&p){std::ifstream in(p,std::ios::binary);check(bool(in),"Read fixture");return {std::istreambuf_iterator<char>(in),{}};}
struct PreferenceGuard {
 bool existed=false,keyExisted=false;DWORD type=0;std::vector<BYTE> value;
 PreferenceGuard(){HKEY key{};keyExisted=RegOpenKeyExW(HKEY_CURRENT_USER,xs::theme::PreferenceKey,0,KEY_QUERY_VALUE|KEY_SET_VALUE,&key)==ERROR_SUCCESS;
  if(keyExisted){DWORD bytes=0;existed=RegQueryValueExW(key,L"Theme",nullptr,&type,nullptr,&bytes)==ERROR_SUCCESS;if(existed){value.resize(bytes);check(RegQueryValueExW(key,L"Theme",nullptr,&type,value.data(),&bytes)==ERROR_SUCCESS,"Read original theme");}RegCloseKey(key);}
  check(xs::theme::save(xs::theme::PreferenceKey,xs::theme::Id::Steel),"Theme registry access required");
 }
 ~PreferenceGuard(){HKEY key{};if(RegOpenKeyExW(HKEY_CURRENT_USER,xs::theme::PreferenceKey,0,KEY_SET_VALUE,&key)==ERROR_SUCCESS){if(existed)RegSetValueExW(key,L"Theme",0,type,value.data(),DWORD(value.size()));else RegDeleteValueW(key,L"Theme");RegCloseKey(key);}if(!keyExisted)RegDeleteKeyW(HKEY_CURRENT_USER,xs::theme::PreferenceKey);}
};
struct Module {
 HMODULE library{};IPluginFactory*factory{};HostApplication host;PClassInfo classes[2]{};
 Module(const char*path){library=LoadLibraryA(path);check(library!=nullptr,"Load candidate DLL");auto init=reinterpret_cast<bool(*)()>(GetProcAddress(library,"InitDll"));if(init)check(init(),"DLL init");auto entry=reinterpret_cast<IPluginFactory*(*)()>(GetProcAddress(library,"GetPluginFactory"));check(entry!=nullptr,"Factory export");factory=entry();check(factory&&factory->countClasses()==2,"Class count");
  PFactoryInfo info{};check(factory->getFactoryInfo(&info)==kResultOk&&std::strcmp(info.vendor,"Phaos")==0,"Vendor");
  FUID expected[2]={{0x7923268C,0x1C9D43DE,0xAA65DAAC,0x78C5232D},{0x0C0B572A,0x7CD34DC5,0x95F21C41,0x22650E26}};
  for(int i=0;i<2;++i){check(factory->getClassInfo(i,&classes[i])==kResultOk,"Class info");check(FUnknownPrivate::iidEqual(classes[i].cid,expected[i].toTUID()),"Class ID changed");}
  check(std::strcmp(classes[0].name,"Xsynth 2000")==0,"Product name");
  IPluginFactory2* f2{};check(factory->queryInterface(IPluginFactory2::iid.toTUID(),reinterpret_cast<void**>(&f2))==kResultOk,"Factory2");PClassInfo2 detail{};f2->getClassInfo2(0,&detail);check(std::strcmp(detail.version,"0.1.0")==0,"Version");f2->release();
 }
 ~Module(){if(factory)factory->release();if(library){auto end=reinterpret_cast<bool(*)()>(GetProcAddress(library,"ExitDll"));if(end)end();FreeLibrary(library);}}
};
struct Handler: IComponentHandler {
 std::map<ParamID,double> pending;
 tresult PLUGIN_API queryInterface(const TUID id,void**out)override{*out=nullptr;if(FUnknownPrivate::iidEqual(id,IComponentHandler::iid)||FUnknownPrivate::iidEqual(id,FUnknown::iid)){*out=this;return kResultOk;}return kNoInterface;}
 uint32 PLUGIN_API addRef()override{return 1;}uint32 PLUGIN_API release()override{return 1;}
 tresult PLUGIN_API beginEdit(ParamID)override{return kResultOk;}
 tresult PLUGIN_API performEdit(ParamID id,ParamValue value)override{pending[id]=value;return kResultOk;}
 tresult PLUGIN_API endEdit(ParamID)override{return kResultOk;}
 tresult PLUGIN_API restartComponent(int32)override{return kResultOk;}
};
struct Instance {
 IComponent* component{};IAudioProcessor* audio{};IEditController*controller{};IPlugView*view{};HWND parent{},child{};Handler handler;
 Instance(Module&m){check(m.factory->createInstance(m.classes[0].cid,IComponent::iid.toTUID(),reinterpret_cast<void**>(&component))==kResultOk,"Create component");check(component->initialize(&m.host)==kResultOk,"Component init");check(component->queryInterface(IAudioProcessor::iid.toTUID(),reinterpret_cast<void**>(&audio))==kResultOk,"Audio interface");check(m.factory->createInstance(m.classes[1].cid,IEditController::iid.toTUID(),reinterpret_cast<void**>(&controller))==kResultOk,"Create controller");check(controller->initialize(&m.host)==kResultOk,"Controller init");controller->setComponentHandler(&handler);
  ProcessSetup setup{};setup.processMode=kRealtime;setup.symbolicSampleSize=kSample32;setup.maxSamplesPerBlock=512;setup.sampleRate=44100;check(audio->setupProcessing(setup)==kResultOk,"Process setup");component->activateBus(kAudio,kOutput,0,true);component->activateBus(kEvent,kInput,0,true);check(component->setActive(true)==kResultOk,"Activate");audio->setProcessing(true);restore(xs::defaults());
 }
 void restoreBytes(std::vector<uint8_t>&bytes){MemoryStream a(bytes.data(),bytes.size());check(component->setState(&a)==kResultOk,"Linked state load");MemoryStream b(bytes.data(),bytes.size());check(controller->setComponentState(&b)==kResultOk,"Linked controller load");handler.pending.clear();}
 void restore(const xs::State&s){auto bytes=xs::encode(s);restoreBytes(bytes);}
 xs::State ui(){xs::State s{};for(int i=0;i<xs::Count;++i)s[i]=controller->getParamNormalized(xs::hostId(i));return s;}
 std::vector<uint8_t> saved(){MemoryStream s;check(component->getState(&s)==kResultOk,"Linked state save");auto*b=reinterpret_cast<uint8_t*>(s.getData());return {b,b+s.getSize()};}
 std::vector<float> process(int count,EventList*events=nullptr,ParameterChanges*extra=nullptr){float left[512]{},right[512]{};float*channels[]={left,right};AudioBusBuffers bus{};bus.numChannels=2;bus.channelBuffers32=channels;ParameterChanges changes;int32 index=0;for(auto&p:handler.pending)changes.addParameterData(p.first,index)->addPoint(0,p.second,index);handler.pending.clear();
  ProcessData data{};data.processMode=kRealtime;data.symbolicSampleSize=kSample32;data.numSamples=count;data.numOutputs=1;data.outputs=&bus;data.inputEvents=events;data.inputParameterChanges=extra?extra:&changes;check(audio->process(data)==kResultOk,"Linked processing");std::vector<float> result(left,left+count);for(int i=0;i<count;++i)check(std::isfinite(left[i])&&std::abs(left[i])<=1&&left[i]==right[i],"Invalid linked audio");return result;
 }
 void open(){check(!view,"Editor already open");parent=CreateWindowW(L"STATIC",L"Xsynth 2000 validation host",WS_POPUP,0,0,960,568,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);check(parent!=nullptr,"Host HWND");view=controller->createView(ViewType::kEditor);check(view&&view->attached(parent,kPlatformTypeHWND)==kResultOk,"Editor attachment");ViewRect r;view->getSize(&r);check(r.getWidth()==960&&r.getHeight()==568,"Native geometry");child=GetWindow(parent,GW_CHILD);check(child!=nullptr,"Native editor");}
 void close(){if(view){view->removed();view->release();view=nullptr;}if(parent){DestroyWindow(parent);parent=nullptr;child=nullptr;}}
 ~Instance(){close();controller->setComponentHandler(nullptr);controller->terminate();controller->release();audio->setProcessing(false);component->setActive(false);audio->release();component->terminate();component->release();}
};
Event note(int pitch,bool on,int offset=0){Event e{};e.type=on?Event::kNoteOnEvent:Event::kNoteOffEvent;e.sampleOffset=offset;if(on){e.noteOn.pitch=pitch;e.noteOn.velocity=.8f;e.noteOn.noteId=pitch;}else{e.noteOff.pitch=pitch;e.noteOff.noteId=pitch;}return e;}
std::vector<float> sequence(Instance&i,const xs::State&s,int pitch,bool chord=false,bool repeated=false,bool allOff=false){i.audio->setProcessing(false);i.component->setActive(false);i.restore(s);i.component->setActive(true);i.audio->setProcessing(true);std::vector<float> result;result.reserve(88200);
 for(int start=0;start<88200;start+=128){int count=std::min(128,88200-start);EventList events;
  for(int at: {0,11025,22050})if((at==0||repeated)&&at>=start&&at<start+count){events.addEvent(note(pitch,true,at-start));if(chord){events.addEvent(note(pitch+4,true,at-start));events.addEvent(note(pitch+7,true,at-start));}}
  if(repeated)for(int at:{10000,21000})if(at>=start&&at<start+count)events.addEvent(note(pitch,false,at-start));
  ParameterChanges changes;int32 index=0;
  if(44100>=start&&44100<start+count){if(allOff)changes.addParameterData(xs::hostId(xs::AllNotesOff),index)->addPoint(44100-start,1,index);else{events.addEvent(note(pitch,false,44100-start));if(chord){events.addEvent(note(pitch+4,false,44100-start));events.addEvent(note(pitch+7,false,44100-start));}}}
  auto out=i.process(count,&events,&changes);result.insert(result.end(),out.begin(),out.end());
 }
 double energy=0,tail=0;for(size_t k=0;k<result.size();++k){energy+=result[k]*result[k];if(k>result.size()-4096)tail+=result[k]*result[k];}check(energy>.001,"Linked silence");check(tail<1e-8,"Stuck voice after release");return result;
}
std::vector<uint32_t> capture(HWND w){BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=960;info.bmiHeader.biHeight=-568;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;HDC dc=CreateCompatibleDC(nullptr);void*data{};auto bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&data,nullptr,0);auto old=SelectObject(dc,bitmap);SendMessageW(w,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);GdiFlush();auto*p=static_cast<uint32_t*>(data);std::vector<uint32_t> out(p,p+960*568);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);return out;}
void comparePng(HWND window,const fs::path&file){auto actual=capture(window);Gdiplus::Bitmap expected(file.c_str());check(expected.GetLastStatus()==Gdiplus::Ok&&expected.GetWidth()==960&&expected.GetHeight()==568,"Expected GUI PNG");Gdiplus::Rect rect(0,0,960,568);Gdiplus::BitmapData data{};check(expected.LockBits(&rect,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&data)==Gdiplus::Ok,"PNG lock");bool equal=true;for(int y=0;y<568;++y){auto*p=reinterpret_cast<uint32_t*>(static_cast<BYTE*>(data.Scan0)+y*data.Stride);for(int x=0;x<960;++x)if((p[x]&0xffffff)!=(actual[y*960+x]&0xffffff))equal=false;}expected.UnlockBits(&data);check(equal,"Linked GUI differs from validated source preview");}
void click(HWND w,int x,int y){SendMessageW(w,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));SendMessageW(w,WM_LBUTTONUP,0,MAKELPARAM(x,y));}

#include "PickerFixtures.h"
#include "HeaderRenderCheck.h"

int main(int argc,char**argv){try {
 check(argc==4,"Expected candidate DLL, expansion root, artifact directory");fs::path root=fs::absolute(argv[2]),art=fs::absolute(argv[3]);fs::create_directories(art);
 Gdiplus::GdiplusStartupInput input;ULONG_PTR token{};check(Gdiplus::GdiplusStartup(&token,&input,nullptr)==Gdiplus::Ok,"GDI+ init");
 {PreferenceGuard preference;Module module(argv[1]);PickerFixtures pickers(module.library);Instance instance(module);
 std::cout<<"PASS: linked identity Xsynth 2000 / Phaos / 0.1.0 and exact processor/controller IDs\n";
 auto basic=xs::defaults();basic[24]=.4;basic[xs::VoiceMode]=1;basic[7]=0;
 for(int n:{36,60,84})sequence(instance,basic,n);sequence(instance,basic,60,true);sequence(instance,basic,60,false,true);sequence(instance,basic,60,false,false,true);
 std::cout<<"PASS: linked low/middle/high, chord, repeated notes, note-off and all-notes-off; finite stereo output and released tails\n";
 for(int id:{0,2})for(double ratio:{.25,.5,1.,2.,4.}){double n=instance.controller->plainParamToNormalized(xs::hostId(id),ratio);closeValue(instance.controller->normalizedParamToPlain(xs::hostId(id),n),ratio,"Linked physical ratio");String128 text{};check(instance.controller->getParamStringByValue(xs::hostId(id),n,text)==kResultOk,"Linked display");auto s=basic;s[id]=n;s[7]=id?1:0;instance.restore(s);instance.process(128);auto saved=instance.saved();xs::State decoded{};check(xs::decode(saved.data(),saved.size(),decoded),"Linked pitch save");closeValue(xs::physical(id,decoded[id]),ratio,"Linked saved pitch");sequence(instance,s,60);}
 for(auto&f:fs::directory_iterator(root/"tests/reference-audio"))if(f.path().extension()==".XseDev"){auto old=read(f.path());xs::State expected{};check(xs::decode(old.data(),old.size(),expected),"V1 fixture");instance.restoreBytes(old);auto ui=instance.ui();check(ui==expected,"Linked migration parameters");instance.process(128);auto current=instance.saved();check(current[4]==2,"Linked current state version");xs::State decoded{};check(xs::decode(current.data(),current.size(),decoded)&&decoded==expected,"Linked migration save");instance.restoreBytes(current);check(instance.ui()==expected,"Linked migrated reload");}
 std::cout<<"PASS: linked both VCO octave stops, physical conversion/display/save/restore, ten V1 migrations and V2 reload\n";
 for(int id:std::initializer_list<int>{0,2,21,xs::XDepth,7,4}){instance.restore(basic);EventList events;events.addEvent(note(60,true));ParameterChanges changes;int32 index=0;auto*lane=changes.addParameterData(xs::hostId(id),index);lane->addPoint(0,0,index);lane->addPoint(127,1,index);instance.process(128,&events,&changes);auto bytes=instance.saved();xs::State state{};check(xs::decode(bytes.data(),bytes.size(),state),"Automation saved state");closeValue(state[id],1,"Automation ID endpoint");}
 std::cout<<"PASS: linked realtime-process-mode automation of both pitches/cutoff/XMOD depth/balance/LFO rate reaches DSP state\n";
 instance.restore(xs::defaults());instance.open();comparePng(instance.child,art/"expected-STEEL.png");click(instance.child,60,24);comparePng(instance.child,art/"expected-about.png");
 auto openAbout=capture(instance.child);click(instance.child,480,250);click(instance.child,480,250);check(linkLaunches==2&&capture(instance.child)==openAbout,"About link dismisses panel or fails repeat launch");
 click(instance.child,480,268);click(instance.child,480,268);check(mailLaunches==2&&capture(instance.child)==openAbout,"Email fails repeat activation or dismisses About");
 click(instance.child,480,280);comparePng(instance.child,art/"expected-STEEL.png");
 std::cout<<"PASS: About has no close button, panel click dismisses; link activates twice without dismissing (system browser substituted)\n";
 for(int theme=0;theme<6;++theme){if(theme){click(instance.child,60,24);click(instance.child,480,492);click(instance.child,60,24);}comparePng(instance.child,art/(std::string("expected-")+xs::theme::Names[theme]+".png"));}
 instance.close();instance.open();comparePng(instance.child,art/"expected-SAND.png");instance.close();
 {Instance reopened(module);reopened.open();comparePng(reopened.child,art/"expected-SAND.png");}
 std::cout<<"PASS: real DLL full 960x568 pixels match six validated themes and About; logo/title/version/credits/geometry; theme persists across editor reopen and fresh instance\n";
 instance.open();
 for(int percent:{100,125,150,200}){int width=960*percent/100,height=568*percent/100;double scale=percent/100.;ViewRect r(0,0,width,height);check(instance.view->onSize(&r)==kResultOk,"Context resize");
 for(int y=0;y<int(std::round(48*scale));++y)for(int x=0;x<width;++x)SendMessageW(instance.child,WM_RBUTTONUP,0,MAKELPARAM(x,y));
 for(int panel:{0,2,7,8}){auto b=xs::context::pixels(xs::panels[panel],scale);expectedContext=xs::context::menuForPanel(panel);int before=contextMenus;SendMessageW(instance.child,WM_RBUTTONUP,0,MAKELPARAM(b.x+b.w/2,b.y+b.h/2));check(contextMenus==before+1,"Linked context dispatch");expectedContext=-1;SendMessageW(instance.child,WM_RBUTTONUP,0,MAKELPARAM(width/2,24));}
 click(instance.child,int(60*scale),int(24*scale));for(auto pt:{POINT{350,90},POINT{480,161},POINT{480,250},POINT{480,268},POINT{480,492}})SendMessageW(instance.child,WM_RBUTTONUP,0,MAKELPARAM(int(pt.x*scale),int(pt.y*scale)));click(instance.child,int(60*scale),int(24*scale));}
 ViewRect native(0,0,960,568);instance.view->onSize(&native);
 factorySelection=xs::factory::Init;click(instance.child,550,28);factorySelection=-1;check(xs::factory::sameSound(instance.ui(),xs::factory::initState()),"Linked INIT menu state");verifyHeaderText(capture(instance.child),960,568,1,xs::theme::Id::Sand,"INIT");
 SendMessageW(instance.child,WM_LBUTTONDBLCLK,MK_LBUTTON,MAKELPARAM(xs::ui::PresetNext.x+12,28));SendMessageW(instance.child,WM_LBUTTONUP,0,MAKELPARAM(xs::ui::PresetNext.x+12,28));check(xs::factory::sameSound(instance.ui(),xs::factory::Presets[0].state),"Linked rapid INIT next lost");
 auto menuBefore=instance.ui();popupChoice=2;injectPointer=true;click(instance.child,40,327);injectPointer=false;popupChoice=-999;auto menuAfter=instance.ui();for(int id=0;id<xs::Count;++id)if(id!=26&&id!=xs::LfoRandom)check(menuBefore[id]==menuAfter[id],"Linked parameter popup click-through");check(GetCapture()!=instance.child,"Linked popup capture stale");
 std::cout<<"PASS: linked INIT, rapid preset click, parameter popup direct/queued EG2 Release click isolation\n";
 std::cout<<"PASS: linked context dispatch, complete scaled headers, stale ownership, About and reentrant popup/file/chooser suppression\n";
 for(int index=0;index<xs::factory::Count;++index){factorySelection=index;click(instance.child,540,28);factorySelection=-1;check(xs::factory::sameSound(instance.ui(),xs::factory::Presets[index].state),"Linked factory selection");check(xs::theme::load(xs::theme::PreferenceKey)==xs::theme::Id::Sand,"Factory changed theme");instance.process(128);}
 click(instance.child,xs::ui::PresetNext.x+12,28);check(xs::factory::sameSound(instance.ui(),xs::factory::initState()),"Linked next wrap");click(instance.child,xs::ui::PresetPrevious.x+12,28);check(xs::factory::sameSound(instance.ui(),xs::factory::Presets[49].state),"Linked previous wrap");
 std::cout<<"PASS: actual linked grouped factory menu, 50 names/states, wrap, theme isolation\n";
 auto preset=basic;preset[0]=0;preset[2]=1;instance.restore(preset);DialogRun save;save.operation=2;save.path=(art/("linked-native-"+std::to_string(GetCurrentProcessId())+".XseDev")).wstring();fileAction(instance,save);check(fs::exists(save.path),"Native Save Patch did not write");
 instance.restore(xs::defaults());DialogRun load;load.path=save.path;fileAction(instance,load);check(instance.ui()==preset,"Native Load Patch differs");instance.process(128);check(xs::theme::load(xs::theme::PreferenceKey)==xs::theme::Id::Sand,"Patch changed theme");
 for(const char*name:{"bass","lead"}){auto shortPath=art/(std::string(name)+".XseDev");fs::copy_file(save.path,shortPath,fs::copy_options::overwrite_existing);load.path=shortPath.wstring();fileAction(instance,load);verifyHeaderText(capture(instance.child),960,568,1,xs::theme::Id::Sand,name);}
 load.path=save.path;
 std::cout<<"PASS: external bass/lead labels use centered production header font at fixed width\n";
 const char* banks[]={"XsynthBank1","XsynthBank3","XsynthBank2018","MoogLikeBank"};int selected[]={11,9,2,0};int total=0;
 for(int b=0;b<4;++b){auto path=root/"tests/dssi/banks"/banks[b];auto text=read(path);auto bank=xs::dssi::parse(std::string(text.begin(),text.end()));DialogRun import;import.operation=1;import.path=path.wstring();import.expectedCount=int(bank.entries.size());import.selected=selected[b];fileAction(instance,import);auto state=instance.ui();for(int id=0;id<xs::Count;++id)if(id!=30&&id!=31&&id!=xs::AllNotesOff&&id!=xs::AllSoundsOff&&id!=xs::Pressure)closeValue(state[id],bank.entries[import.selected].state[id],"Linked imported state mismatch");instance.process(128);if(b==0){DialogRun converted;converted.operation=2;converted.path=(art/"Converted Lead.XseDev").wstring();fileAction(instance,converted);verifyHeaderText(capture(instance.child),960,568,1,xs::theme::Id::Sand,"Converted Lead");converted.operation=0;fileAction(instance,converted);verifyHeaderText(capture(instance.child),960,568,1,xs::theme::Id::Sand,"Converted Lead");}total+=import.expectedCount;std::cout<<"PASS: linked import "<<banks[b]<<" chooser="<<import.expectedCount<<" all available, names/notes and applied selected state\n";}
 auto before=instance.ui();DialogRun cancel;cancel.operation=1;cancel.path=(root/"tests/dssi/banks/XsynthBank1").wstring();cancel.expectedCount=60;cancel.cancel=true;fileAction(instance,cancel);check(instance.ui()==before,"Cancel changed state");check(total==239,"Linked corpus count");
 factorySelection=6;click(instance.child,540,28);factorySelection=-1;check(xs::factory::sameSound(instance.ui(),xs::factory::Presets[6].state),"Factory after external");
 auto immutable=xs::factory::Presets[6].state;fileAction(instance,save);check(xs::factory::Presets[6].state==immutable,"Save altered factory");instance.restore(xs::defaults());fileAction(instance,load);check(xs::factory::sameSound(instance.ui(),immutable),"Saved factory reload");
 DialogRun renamed;renamed.operation=2;renamed.path=(art/"My Strings.XseDev").wstring();fileAction(instance,renamed);verifyHeaderText(capture(instance.child),960,568,1,xs::theme::Id::Sand,"My Strings");renamed.operation=0;fileAction(instance,renamed);verifyHeaderText(capture(instance.child),960,568,1,xs::theme::Id::Sand,"My Strings");fs::copy_file(art/"My Strings.XseDev",art/"Renamed Strings.XseDev",fs::copy_options::overwrite_existing);renamed.path=(art/"Renamed Strings.XseDev").wstring();fileAction(instance,renamed);verifyHeaderText(capture(instance.child),960,568,1,xs::theme::Id::Sand,"Renamed Strings");
 std::cout<<"PASS: linked filename authority: factory My Strings, DSSI Converted Lead, external Renamed Strings after Save/Load\n";
 std::cout<<"PASS: real native Save/Load Patch with headless OS picker fixtures; four DSSI file actions, 239 chooser entries selectable, Cancel atomic\n";
 instance.close();
 {Instance third(module);auto changed=basic;changed[0]=0;third.restore(changed);std::vector<float> b;{Instance second(module);second.restore(basic);auto a=sequence(second,basic,60);b=sequence(third,changed,60);check(a!=b,"Instances share DSP");check(second.ui()==basic&&third.ui()==changed,"Instances share patch state");second.open();}auto after=sequence(third,changed,60);check(after==b,"Closing other instance affected DSP");}
 std::cout<<"PASS: three concurrent instances, independent state/audio, closing one leaves another working\n";
 }
 Gdiplus::GdiplusShutdown(token);std::cout<<"RESULT: linked validation candidate technical checks PASS; human listening and genuine DSSI A/B not performed\n";return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}

