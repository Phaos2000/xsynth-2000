// Xsynth port. GPL-2.0-or-later; original DSP: (C) 1999 S. J. Brookes.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include "ExpansionParameters.h"
#include "DigitalOscillator.h"
#include "Transitions.h"

namespace xs {

constexpr double Pi = 3.14159265358979323846;
struct Spec {
  const char *name;
  int x, y;
  double a, b, c;
  bool log;
  double initial;
};
inline constexpr Spec specs[24] = {
    {"pitch", 35, 30, .25, 2, 4, true, .5},
    {"pulsewidth", 35, 170, 0, 1, 0, false, .5},
    {"pitch", 35, 270, .25, 2, 4, true, .5},
    {"pulsewidth", 35, 410, 0, 1, 0, false, .5},
    {"lfo frequency", 135, 30, .1, 10, 2, true, 0},
    {"pitch depth", 135, 170, 0, 1, 0, false, 0},
    {"filter depth", 135, 240, 0, 1, 0, false, 0},
    {"balance", 135, 340, 0, 1, 0, false, .5},
    {"glide time", 135, 440, 1, .01, 1, true, 0},
    {"attack time", 235, 30, .1, 10, -4, true, 0},
    {"decay time", 235, 100, .1, 10, -4, true, 0},
    {"sustain level", 235, 170, 0, 1, 0, false, 1},
    {"release time", 235, 240, .1, 10, -4, true, 0},
    {"pitch depth", 235, 310, 0, 1, 0, false, 0},
    {"filter depth", 235, 380, 0, 50, 0, false, 0},
    {"attack time", 335, 30, .1, 10, -4, true, 0},
    {"decay time", 335, 100, .1, 10, -4, true, 0},
    {"sustain level", 335, 170, 0, 1, 0, false, 1},
    {"release time", 335, 240, .1, 10, -4, true, 0},
    {"pitch depth", 335, 310, 0, 1, 0, false, 0},
    {"filter depth", 335, 380, 0, 50, 0, false, 0},
    {"cutoff", 435, 30, 0, 50, 0, false, 1},
    {"resonance", 435, 100, 0, -1.995, 2, false, 0},
    {"volume", 435, 270, 0, 1, 0, false, .5}};
inline double physical(int i, double n) {
  const auto &s = specs[i];
  return s.log ? s.a * std::pow(s.b, s.c * n) : (s.a * n + s.b) * n + s.c;
}
inline double normalized(int i, double v) {
  const auto &s = specs[i];
  double n =
      s.log ? std::log(v / s.a) / (s.c * std::log(s.b)) : (v - s.c) / s.b;
  return std::clamp(n, 0., 1.);
}
using State = std::array<double, Count>;
inline State defaults() {
  State s{};
  for (int i = 0; i < 24; ++i)
    s[i] = specs[i].initial;
  s[29] = 1;
  s[30] = .5;
  for (int i=34;i<Count;++i) s[i]=extraDefault(i);
  return s;
}
inline bool valid(double d) { return std::isfinite(d) && d >= 0 && d <= 1; }
inline double stepped(int id, double value) {
  int steps=stepsFor(id);
  if(steps) return std::round(value*steps)/steps;
  return value;
}
inline void append32(std::vector<uint8_t> &b, uint32_t n) {
  for (int i = 0; i < 4; ++i)
    b.push_back(uint8_t(n >> (8 * i)));
}
inline uint32_t get32(const uint8_t *p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
         (uint32_t(p[3]) << 24);
}
inline float getFloat(const uint8_t *p) {
  auto n = get32(p);
  float f;
  std::memcpy(&f, &n, 4);
  return f;
}
inline void putFloat(std::vector<uint8_t> &b, float f) {
  uint32_t n;
  std::memcpy(&n, &f, 4);
  append32(b, n);
}
inline bool importPatch(const std::vector<uint8_t> &b, State &out) {
  if (b.size() != 209)
    return false;
  auto s = defaults();
  for (int i = 0; i < 24; ++i) {
    double v = getFloat(&b[i * 8 + 4]);
    double lo = std::min(physical(i, 0), physical(i, 1)),
           hi = std::max(physical(i, 0), physical(i, 1));
    if (!std::isfinite(v) || v < lo - 1.e-6 || v > hi + 1.e-5)
      return false;
    s[i] = normalized(i, std::clamp(v, lo, hi));
  }
  for (int i = 0; i < 3; ++i) {
    auto w = b[192 + i * 5 + 4];
    int k = 0;
    while (k < 6 && (1 << k) != w)
      ++k;
    if (k == 6)
      return false;
    s[24 + i] = k / 5.;
  }
  if (b[207] > 1 || b[208] > 1)
    return false;
  s[27] = b[207];
  s[28] = b[208];
  out = s;
  return true;
}
inline std::vector<uint8_t> exportPatch(const State &s) {
  std::vector<uint8_t> b;
  for (int i = 0; i < 24; ++i) {
    putFloat(b, float(1.25 * Pi - s[i] * 1.5 * Pi));
    putFloat(b, float(physical(i, s[i])));
  }
  for (int i = 0; i < 3; ++i) {
    int k = int(std::round(s[24 + i] * 5));
    putFloat(b, float(1.25 * Pi - k / 5. * 1.5 * Pi));
    b.push_back(uint8_t(1 << k));
  }
  b.push_back(s[27] >= .5);
  b.push_back(s[28] >= .5);
  return b;
}
constexpr size_t StateBytes = 8 + Count * 8;
// V2 changes VCO pitch normalization; V3 migrates the modulation destination enumeration.
// V1 ratio=2^(2*n); V2 ratio=.25*2^(4*n). Old n becomes .5+.5*n.
inline int stateValueCount(const uint8_t* header) {
  if(!std::memcmp(header,"XSV3\1\0\0\0",8))return LegacyCount;
  if(!std::memcmp(header,"XSED\5\0\0\0",8))return Count;
  if(!std::memcmp(header,"XSED\4\0\0\0",8))return PreUnisonCount;
  if(!std::memcmp(header,"XSED\1\0\0\0",8)||!std::memcmp(header,"XSED\2\0\0\0",8)||!std::memcmp(header,"XSED\3\0\0\0",8))return PreviousCount;
  return 0;
}
inline std::vector<uint8_t> encode(const State &s) {
  std::vector<uint8_t> b = {'X', 'S', 'E', 'D', 5, 0, 0, 0};
  for (auto v : s) {
    uint64_t n;
    std::memcpy(&n, &v, 8);
    for (int j = 0; j < 8; ++j)
      b.push_back(uint8_t(n >> (8 * j)));
  }
  return b;
}
inline bool decode(const uint8_t *b, size_t size, State &out) {
  if(size<8)return false;
  int count=stateValueCount(b);
  if(!count || size!=8+size_t(count)*8)return false;
  State s=defaults();
  for (int i = 0; i < count; ++i) {
    uint64_t n = 0;
    for (int j = 0; j < 8; ++j)
      n |= uint64_t(b[8 + i * 8 + j]) << (8 * j);
    std::memcpy(&s[i], &n, 8);
    if (!valid(s[i]))
      return false;
  }
  if(b[4]==1) { s[0]=.5+.5*s[0];s[2]=.5+.5*s[2]; }
  // V1/V2 retain their exact legacy normalized values. V3 used index/40:
  // migrate to a canonical legacy value where possible, otherwise an override.
  if(b[4]==3 && count==PreviousCount)for(int slot=0;slot<6;++slot){
    int id=legacyDestinationId(slot),destination=int(std::round(s[id]*(EstablishedDestinationCount-1)));
    s[id]=destination<LegacyDestinationCount?destination/double(LegacyDestinationCount-1):0.;
    s[ExpandedDestinationFirst+slot]=destination<LegacyDestinationCount?0.:(destination+1)/double(EstablishedDestinationCount);
  }
  out = s;
  return true;
}

class LegacyEngine {
  double sr = 44100;
  struct Tables {
    std::array<float,101> sine{},tri{},vol{};
    Tables() { for(int i=0;i<=100;++i) {
      sine[i]=float(std::sin(2*Pi*i/100));
      tri[i]=i<25?i/25.f:i<75?1-2*(i-25)/50.f:(i-75)/25.f-1;
      vol[i]=float(std::pow(i/100.,1/(2*std::log10(2.))));
    }}
  };
  inline static const Tables tables{};
  std::array<bool, 128> notes{};
  float pos[3]{}, eg[2]{}, d[4]{}, pitch = 0, previousPitch = 0, baseFilter = 0;
  int phase[2]{}, previousNote = 0, tick = 0;
  bool active = false, sync[3]{};
  uint32_t dirtyMask = (1u<<24)-1;
  float p[24]{}, rates[2][3]{}, glide = 1,lastFilterCoefficient=0;
  SignalBlend<6> waveform[2];SignalBlend<2> engines[2];
  float evaluatedWave(float phase,int kind,float pw) const {
    float w=phase*100;int i=std::min(99,int(w));float f=w-i;
    switch(kind){case 0:return tables.sine[i]+(tables.sine[i+1]-tables.sine[i])*f;
      case 1:return tables.tri[i]+(tables.tri[i+1]-tables.tri[i])*f;
      case 2:return 2*phase-1;case 3:return 1-2*phase;
      case 4:return phase<.5f?1.f:-1.f;default:return phase<pw?1.f:-1.f;}
  }
  float wave(int osc, float frequency, float dt, int kind, float pw) {
    pos[osc] += dt * frequency;
    if (pos[osc] >= 1.f) {
      pos[osc] -= std::floor(pos[osc]);
      sync[osc] = true;
    }
    float p = pos[osc], w = p * 100;
    int i = std::min(99, int(w));
    float f = w - i;
    if(osc<2 && (waveform[osc].remaining || engines[osc].remaining)) {
      float sample=0;
      for(int k=0;k<6;++k)if(waveform[osc].weights[k]!=0) {
        float raw=evaluatedWave(p,k,pw),clean=oscillator::clean(p,dt*frequency,k,pw,raw);
        sample+=waveform[osc].weights[k]*(engines[osc].weights[0]*raw+engines[osc].weights[1]*clean);
      }
      return sample;
    }
    if(osc<2 && state[Engine1+osc]>=.5)
      return oscillator::clean(p,dt*frequency,kind,pw,evaluatedWave(p,kind,pw));

    switch (kind) {
    case 0:
      return tables.sine[i] + (tables.sine[i + 1] - tables.sine[i]) * f;
    case 1:
      return tables.tri[i] + (tables.tri[i + 1] - tables.tri[i]) * f;
    case 2:
      return 2 * p - 1;
    case 3:
      return 1 - 2 * p;
    case 4:
      return p < .5f ? 1.f : -1.f;
    default:
      return p < pw ? 1.f : -1.f;
    }
  }

protected:
  struct Continuity {float oscillator[2],filter[4],env[2],hz,lfo,coefficient;int stage[2];bool active;};
  Continuity continuity() const {
    Continuity c{};std::copy_n(pos,2,c.oscillator);std::copy_n(d,4,c.filter);
    std::copy_n(eg,2,c.env);std::copy_n(phase,2,c.stage);
    c.hz=pitch;c.lfo=pos[2];c.coefficient=lastFilterCoefficient;c.active=active;return c;
  }

public:
  State state = defaults();
  LegacyEngine() = default;
  static float sineSample(float phase) {
    float x=phase*100;int i=std::min(99,int(x));
    return tables.sine[i]+(tables.sine[i+1]-tables.sine[i])*(x-i);
  }
  void reset(double rate) {
    sr = rate;
    dirtyMask = (1u<<24)-1;
    notes.fill(false);
    std::fill_n(pos, 3, 0.f);
    std::fill_n(eg, 2, 0.f);
    std::fill_n(d, 4, 0.f);
    std::fill_n(phase, 2, 0);
    std::fill_n(sync, 3, false);
    pitch = previousPitch = baseFilter = 0;
    previousNote = tick = 0;
    active = false;
    for(int o=0;o<2;++o){waveform[o].reset(discrete(24+o,state[24+o]));engines[o].reset(state[Engine1+o]>=.5);}
  }
  void note(int n, bool down) {
    if (n >= 0 && n < 128) {
      notes[n] = down;
      tick = 0;
    }
  }
  void allOff() {
    notes.fill(false);
    tick = 0;
  }
  void set(int id, double v) {
    if (id >= 0 && id < Count && valid(v)) {
      v=stepped(id,v);
      if(state[id]==v) return;
      state[id] = v;
      int samples=std::max(1,int(sr*.002));
      if(id==24||id==25){int o=id-24;if(active)waveform[o].select(discrete(id,v),samples);else waveform[o].reset(discrete(id,v));}
      if(id==Engine1||id==Engine2){int o=id-Engine1;if(active)engines[o].select(v>=.5,samples);else engines[o].reset(v>=.5);}
      if(id<24)dirtyMask|=1u<<id;
      if (id == 30 || id == 31)
        tick = 0;
    }
  }
  int heldNote() const { return previousNote; }
  float next() {
    if (dirtyMask) {
      for (int i=0;i<24;++i)if(dirtyMask&(1u<<i))p[i]=float(physical(i,state[i]));
      if(dirtyMask&(1u<<8))glide=sr==44100?p[8]:float(1-std::pow(1.-p[8],44100/sr));
      for(int e=0;e<2;++e)for(int j=0;j<3;++j) {
        int id=(e?15:9)+(j==2?3:j);
        if(dirtyMask&(1u<<id))rates[e][j]=sr==44100?p[id]:float(1-std::pow(1.-p[id],44100/sr));
      }
      dirtyMask=0;
    }
    if (tick == 0) {
      int n = 96;
      while (n > 20 && !notes[n])
        --n;
      if (n > 20) {
        float target = float(440 * std::pow(2., (n - 69) / 12.));
        if (previousNote) {
          pitch = glide * target + (1 - glide) * previousPitch;
          if (n != previousNote)
            phase[0] = phase[1] = 0;
        } else {
          pitch = target;
          phase[0] = phase[1] = 0;
        }
        previousNote = n;
        previousPitch = pitch;
        pitch *= float(std::exp((state[30] * 2 - 1) * .69314718));
        baseFilter = float(2 * Pi / sr * pitch * (1 - state[31]));
        active = true;
      } else {
        previousNote = 0;
        if (eg[0] > 1.e-2f)
          phase[0] = phase[1] = 2;
        else {
          active = false;
          std::fill_n(d, 4, 0.f);
        }
      }
    }
    tick = (tick + 1) % 256;
    if (!active)
      return 0;
    for(int o=0;o<2;++o){waveform[o].advance();engines[o].advance();}
    float dt = float(1 / sr);
    float lfo = wave(2, p[4], dt, int(std::round(state[26] * 5)), .25f);
    for (int e = 0; e < 2; ++e) {
      int start = e ? 15 : 9;
      float a = rates[e][phase[e]];
      float target = phase[e] == 0 ? 1 : phase[e] == 1 ? p[start + 2] : 0;
      eg[e] = a * target + (1 - a) * eg[e];
      if (phase[e] == 0 && eg[e] > .99f)
        phase[e] = 1;
    }
    float o1 = wave(0, p[0] * pitch, dt, int(std::round(state[24] * 5)), p[1]),
          dt2 = dt;
    if (state[27] >= .5 && sync[0]) {
      sync[0] = false;
      pos[1] = 0;
      dt2 = pos[0] / (p[0] * pitch);
    }
    float o2 = wave(1,
                    p[2] * pitch * (1 + eg[0] * p[13]) * (1 + eg[1] * p[19]) *
                        (1 + lfo * p[5]),
                    dt2, int(std::round(state[25] * 5)), p[3]);
    float input = (1 - p[7]) * o1 + p[7] * o2;
    float cutoff =
        std::min(.825f, (baseFilter * p[21] + baseFilter * p[14] * eg[0] +
                         baseFilter * p[20] * eg[1]) *
                            (1 + lfo * p[6]));
    lastFilterCoefficient=cutoff;
    d[1] += cutoff * d[0];
    float high = input - d[1] - p[22] * d[0];
    d[0] += cutoff * high;
    float out = d[1];
    if (state[28] >= .5) {
      d[3] += cutoff * d[2];
      high = out - d[3] - p[22] * d[2];
      d[2] += cutoff * high;
      out = d[3];
    }
    float w = p[23] * 100;
    int k = std::min(99, int(w));
    float gain = tables.vol[k] + (tables.vol[k + 1] - tables.vol[k]) * (w - k);
    out *= eg[0] * gain;
    if (!std::isfinite(out)) {
      std::fill_n(d, 4, 0.f);
      return 0;
    }
    return state[29] >= .5 ? std::clamp(out, -1.f, 1.f) : 0.f;
  }
};
} // namespace xs

#include "ExpandedEngine.h"
