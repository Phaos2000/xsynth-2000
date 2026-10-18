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

#define number_of_points 101
#define number_of_points_m1 100

static float factor[number_of_points];
static float deriva[number_of_points];

void volume_init(void)
{
  int i;
  float volume,volume_exponent=1./(2.*log10(2.));

  for(i=0; i<number_of_points; ++i)
  {
    volume=(float)i/number_of_points_m1;
    factor[i]=pow(volume,volume_exponent);
  }

  for(i=0; i<number_of_points_m1; ++i)
  {
    deriva[i]=factor[i+1]-factor[i];
  }
}

float volume(float level)
{
  unsigned char segment;
  float fract;

  segment=number_of_points_m1*level;
  fract=number_of_points_m1*level-segment;

  if(segment==number_of_points_m1)
    return 1.;
  else
    return factor[segment]+deriva[segment]*fract;
}
