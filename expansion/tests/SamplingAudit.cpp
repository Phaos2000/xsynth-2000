// GPL-2.0-or-later. Offline Windows thread-context sampling, never in plugin.
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include "Core.h"
#include <thread>
#include <atomic>
#include <map>
#include <string>
#include <fstream>
#include <iostream>
int main(int argc,char**argv){
 HANDLE process=GetCurrentProcess();SymSetOptions(SYMOPT_LOAD_LINES|SYMOPT_UNDNAME|SYMOPT_DEFERRED_LOADS);if(!SymInitialize(process,nullptr,TRUE))return 1;
 std::ofstream csv(argc>1?argv[1]:"sampling.csv");csv<<"case,location,samples\n";
 for(int mode=0;mode<6;++mode){std::atomic<bool>done{false},ready{false};std::map<std::string,int>samples;
 std::thread worker([&]{_mm_setcsr(_mm_getcsr()|0x8040);xs::Engine e;e.reset(96000);e.set(xs::VoiceMode,1);e.set(xs::Voices,1);e.set(23,.15);e.set(24,.4);e.set(25,1);e.set(xs::Engine1,mode%2);e.set(xs::Engine2,mode%2);e.set(xs::Keytrack,0);e.set(21,mode>=4?1:.15);e.set(28,1);if(mode>=2){for(int l=0;l<4;++l){e.set(xs::lfoParam(l,1),.01);e.set(xs::lfoParam(l,2),.02);}for(int slot=0;slot<12;++slot){e.set(xs::matrixId(slot),.1);if(slot%4)e.set(xs::SourceExtensionFirst+slot,(slot%4)/3.);e.set(xs::matrixId(slot,2),.51);}}if(mode>=4){e.set(xs::Unison,1);e.set(xs::Detune,.5);}for(int n=0;n<(mode>=4?4:32);++n)e.note(36+n,true,.7f,n);for(int i=0;i<9600;++i)e.next();ready=true;float sum=0;while(!done)sum+=e.next();volatile float sink=sum;(void)sink;});
 while(!ready)Sleep(1);HANDLE thread=worker.native_handle();
 for(int sample=0;sample<2000;++sample){Sleep(1);if(SuspendThread(thread)==DWORD(-1))return 2;CONTEXT context{};context.ContextFlags=CONTEXT_FULL;GetThreadContext(thread,&context);STACKFRAME64 stack{};stack.AddrPC.Offset=context.Rip;stack.AddrPC.Mode=AddrModeFlat;stack.AddrFrame.Offset=context.Rbp;stack.AddrFrame.Mode=AddrModeFlat;stack.AddrStack.Offset=context.Rsp;stack.AddrStack.Mode=AddrModeFlat;
 std::string location;for(int frame=0;frame<12;++frame){DWORD64 address=frame?stack.AddrPC.Offset:context.Rip;char storage[sizeof(SYMBOL_INFO)+1024]{};auto*symbol=reinterpret_cast<SYMBOL_INFO*>(storage);symbol->SizeOfStruct=sizeof(SYMBOL_INFO);symbol->MaxNameLen=1023;DWORD64 displacement=0;std::string name=SymFromAddr(process,address,&displacement,symbol)?symbol->Name:"unknown";IMAGEHLP_LINE64 line{};line.SizeOfStruct=sizeof(line);DWORD delta=0;
 if(SymGetLineFromAddr64(process,address,&delta,&line)){std::string file=line.FileName;if(file.find("ExpandedEngine.h")!=std::string::npos||file.find("UnisonGain.h")!=std::string::npos||file.find("DigitalOscillator.h")!=std::string::npos){location=name+" @ "+file.substr(file.find_last_of("\\/")+1)+":"+std::to_string(line.LineNumber);break;}}
 if(frame==0)location=name;
 if(!StackWalk64(IMAGE_FILE_MACHINE_AMD64,process,thread,&stack,&context,nullptr,SymFunctionTableAccess64,SymGetModuleBase64,nullptr))break;}
 ResumeThread(thread);++samples[location];}
 done=true;worker.join();for(auto&entry:samples)csv<<mode<<','<<entry.first<<','<<entry.second<<'\n';csv.flush();}
 SymCleanup(process);return 0;
}
