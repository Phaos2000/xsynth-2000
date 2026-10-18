/*
 * Xsynth - a real-time software synthesizer
 *
 * Copyright (C) 1999 S. J. Brookes
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License in the file COPYING for more details.
 */

#include<math.h>
#include"synth.h"

static float sine_wave[WAVE_POINTS+1],
             triangle_wave[WAVE_POINTS+1];

void init_waveforms(void)
{
  int i,qn,tqn;

  for(i=0;i<=WAVE_POINTS;++i)
  {
    sine_wave[i]=sin(2.*PI*(float)i/WAVE_POINTS);
  }

  qn=WAVE_POINTS/4;
  tqn=3*WAVE_POINTS/4;

  for(i=0;i<=WAVE_POINTS;++i)
  {
    if(i < qn)
      triangle_wave[i]=(float)i/(float)qn;
    else
    {
      if(i < tqn)
        triangle_wave[i]=1.-2.*(float)(i-qn)/(float)(tqn-qn);
      else
        triangle_wave[i]=(float)(i-tqn)/(float)(WAVE_POINTS-tqn)-1.;
    }
  }
}

float oscillator(float *pos,float omega,float deltat,
                 unsigned char waveform,float pw,unsigned char *sync)
{
  float wpos,f;
  unsigned char i;

  *pos+=deltat*omega;

  if(*pos>=1.)
  {
    *pos-=1.;
    *sync=1;
  }

  switch(waveform)
  {
  case 0x01:                                                    /* sine wave */
    wpos=*pos*WAVE_POINTS;
    i=(unsigned char)wpos;
    f=wpos-(float)i;
    return (sine_wave[i]+(sine_wave[i+1]-sine_wave[i])*f);

  case 0x02:                                                /* triangle wave */
    wpos=*pos*WAVE_POINTS;
    i=(unsigned char)wpos;
    f=wpos-(float)i;
    return (triangle_wave[i]+(triangle_wave[i+1]-triangle_wave[i])*f);

  case 0x04:                                             /* up sawtooth wave */
    return (*pos*2.-1.);

  case 0x08:                                           /* down sawtooth wave */
    return (1.-*pos*2.);

  case 0x10:                                                  /* square wave */
    return ((*pos<0.5) ? 1.0 : -1.0);

  case 0x20:                                                   /* pulse wave */
    return ((*pos<pw) ? 1.0 : -1.0);
  }
}
