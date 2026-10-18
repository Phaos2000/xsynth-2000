# Adapt only platform I/O around the original synth function. Its DSP statements
# are read directly from the untouched 1999 source at configure time.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/synth.c" legacy)
string(REPLACE "\r\n" "\n" legacy "${legacy}")
string(FIND "${legacy}" "void synth(" first)
string(FIND "${legacy}" "/* function to shut down synth engine */" last)
math(EXPR length "${last}-${first}")
string(SUBSTRING "${legacy}" ${first} ${length} legacy)
string(REPLACE "void synth(value *vptr,midi_info *mptr)" "std::vector<float> legacyRender(value *vptr,midi_info *mptr)" legacy "${legacy}")
string(REPLACE "  int fd;" "  std::vector<float> rendered;" legacy "${legacy}")
string(REPLACE "register int i;" "int i;" legacy "${legacy}")
string(REPLACE "signed short sample[BUFF_SIZE],silence[BUFF_SIZE]={0};" "float sample[BUFF_SIZE],silence[BUFF_SIZE]={0};" legacy "${legacy}")
string(REPLACE "float lfo,omega3,lfo_pos," "float lfo,omega3,lfo_pos=0.," legacy "${legacy}")
string(REPLACE "float eg1,eg1_rate" "float eg1=0.,eg1_rate" legacy "${legacy}")
string(REPLACE "float eg2,eg2_rate" "float eg2=0.,eg2_rate" legacy "${legacy}")
string(FIND "${legacy}" "  if(signal(" platform_start)
string(FIND "${legacy}" "  pitch_init(pitch);" platform_end)
math(EXPR platform_length "${platform_end}-${platform_start}")
string(SUBSTRING "${legacy}" ${platform_start} ${platform_length} platform)
string(REPLACE "${platform}" "" legacy "${legacy}")
string(REPLACE "  printf(\"\\nXsynth - a real-time software synthesizer\\n\");" "" legacy "${legacy}")
# Compare floating DSP before the legacy int16 device conversion. Clipping is
# applied equally by the test; the port's safer output conversion is documented.
string(REPLACE "(signed short) 0x7fff*output" "output" legacy "${legacy}")
string(REPLACE "write(fd,silence,BUFF_SIZE*sizeof(short int));" "rendered.insert(rendered.end(),silence,silence+BUFF_SIZE); if(rendered.size()>=32768)return rendered;" legacy "${legacy}")
string(REPLACE "write(fd,sample,BUFF_SIZE*sizeof(short int));" "rendered.insert(rendered.end(),sample,sample+BUFF_SIZE); if(rendered.size()>=32768)return rendered;" legacy "${legacy}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/LegacyReference.cpp"
 "// Generated from original GPL-2.0-or-later synth.c; see tests/LegacyReference.cmake.\n#include <cmath>\n#include <vector>\n#include \"synth.h\"\n#include \"LegacyReference.h\"\n#define FREQMAX 0.825\n${legacy}")
