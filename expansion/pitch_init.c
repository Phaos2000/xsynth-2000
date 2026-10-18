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

#define ref_pitch 440.0
#define ref_note 69

void pitch_init(float *pptr)
{
  int i;

  float pexp;

  for(i=0;i<128;++i)
  {
    pexp=(float)(i-ref_note)/12.;
    *(pptr+i)=ref_pitch*pow(2.,pexp);
  }
}
