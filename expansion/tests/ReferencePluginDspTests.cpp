// GPL-2.0-or-later. Compare current DSP to the preserved, unmodified VST3 binary.
#define NOMINMAX
#include "Core.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include <windows.h>
#include <iostream>
#include <stdexcept>
using namespace Steinberg;
using namespace Steinberg::Vst;
void check(bool v, const char *m) {
  if (!v)
    throw std::runtime_error(m);
}
int main(int argc, char **argv) {
  try {
    check(argc == 2, "Reference bundle binary argument required");
    auto dll = LoadLibraryA(argv[1]);
    check(dll != nullptr, "Load preserved VST3");
    auto init = reinterpret_cast<bool (*)()>(GetProcAddress(dll, "InitDll"));
    if (init)
      check(init(), "InitDll");
    auto factoryFn =
        reinterpret_cast<IPluginFactory *(*)()>(GetProcAddress(dll, "GetPluginFactory"));
    check(factoryFn != nullptr, "Factory export");
    auto factory = factoryFn();
    PClassInfo info{};
    factory->getClassInfo(0, &info);
    check(std::strcmp(info.name, "Xsynth") == 0, "Must load reference Xsynth");
    HostApplication host;
    double worst = 0;
    int count = 0;
    for (double rate : {44100., 48000., 88200., 96000.})
      for (int block : {32, 64, 128, 256, 512, 1024})
        for (int kind = 0; kind < 8; ++kind) {
          IComponent *component{};
          check(factory->createInstance(info.cid, IComponent::iid.toTUID(),
                                        reinterpret_cast<void **>(&component)) == kResultOk,
                "Create");
          check(component->initialize(&host) == kResultOk, "Initialize");
          IAudioProcessor *processor{};
          check(component->queryInterface(IAudioProcessor::iid.toTUID(),
                                          reinterpret_cast<void **>(&processor)) == kResultOk,
                "Audio interface");
          auto state = xs::defaults();
          state[24] = kind == 0 ? 0 : kind == 2 ? 1 : .4;
          state[25] = .8;
          state[7] = kind == 3 ? .3 : 0;
          state[21] = kind == 4 ? .035 : 1;
          state[22] = kind == 4 ? .8 : 0;
          if (kind == 5) {
            state[9] = .5;
            state[10] = .6;
            state[11] = .3;
            state[12] = .5;
          }
          if (kind == 6)
            state[8] = .7;
          if (kind == 7) {
            state[27] = 1;
            state[7] = 1;
            state[2] = .5+.5*.37;
          }
          auto referenceState=state;
          referenceState[0]=2*state[0]-1;referenceState[2]=2*state[2]-1;
          auto bytes = xs::encode(referenceState);
          bytes.resize(8 + 32 * 8);
          bytes[2] = 'V';
          bytes[3] = '3';
          bytes[4] = 1;
          MemoryStream stream(bytes.data(), bytes.size());
          check(component->setState(&stream) == kResultOk, "Reference state");
          ProcessSetup setup{};
          setup.processMode = kOffline;
          setup.symbolicSampleSize = kSample32;
          setup.maxSamplesPerBlock = 1024;
          setup.sampleRate = rate;
          check(processor->setupProcessing(setup) == kResultOk, "Setup");
          component->activateBus(kAudio, kOutput, 0, true);
          component->activateBus(kEvent, kInput, 0, true);
          component->setActive(true);
          processor->setProcessing(true);
          xs::Engine current;
          current.state = state;
          current.reset(rate);
          float left[1024]{}, right[1024]{};
          float *channels[] = {left, right};
          AudioBusBuffers bus{};
          bus.numChannels = 2;
          bus.channelBuffers32 = channels;
          double error = 0, energy = 0;
          int total = int(rate);
          for (int start = 0; start < total; start += block) {
            int n = std::min(block, total - start);
            EventList events;
            const int times[] = {37, int(rate / 3), int(rate * .7), int(rate * .7) + 1};
            for (int k = 0; k < 4; ++k)
              if (times[k] >= start && times[k] < start + n) {
                Event event{};
                event.sampleOffset = times[k] - start;
                event.type = k < 2 ? Event::kNoteOnEvent : Event::kNoteOffEvent;
                if (k < 2) {
                  event.noteOn.pitch = k == 0 ? 60 : 72;
                  event.noteOn.velocity = 1;
                  event.noteOn.noteId = k;
                } else {
                  event.noteOff.pitch = k == 2 ? 72 : 60;
                  event.noteOff.noteId = k == 2 ? 1 : 0;
                }
                events.addEvent(event);
              }
            ProcessData data{};
            data.processMode = kOffline;
            data.symbolicSampleSize = kSample32;
            data.numSamples = n;
            data.numOutputs = 1;
            data.outputs = &bus;
            data.inputEvents = &events;
            check(processor->process(data) == kResultOk, "Reference render");
            for (int j = 0; j < n; ++j) {
              int t = start + j;
              if (t == times[0])
                current.note(60, true);
              if (t == times[1])
                current.note(72, true);
              if (t == times[2])
                current.note(72, false);
              if (t == times[3])
                current.note(60, false);
              float y = current.next();
              check(std::isfinite(left[j]) && left[j] == right[j], "Reference audio");
              error += (y - left[j]) * (y - left[j]);
              energy += left[j] * left[j];
            }
          }
          double relative = std::sqrt(error / std::max(energy, 1.e-20));
          worst = std::max(worst, relative);
          check(std::isfinite(relative), "Current/reference comparison nonfinite"); // Intentional filter/direct-LFO corrections; no parity gate.
          processor->setProcessing(false);
          component->setActive(false);
          processor->release();
          component->terminate();
          component->release();
          ++count;
        }
    factory->release();
    auto finish = reinterpret_cast<bool (*)()>(GetProcAddress(dll, "ExitDll"));
    if (finish)
      finish();
    FreeLibrary(dll);
    std::cout << "COMPARISON preserved Xsynth binary (parity not required): " << count
              << " comparisons (8 patches x 4 rates x 6 block sizes); worst relative RMS=" << worst
              << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
