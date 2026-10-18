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

#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<sys/ioctl.h>
#include<sys/soundcard.h>

int init_dsp(int format,int stereo,float *freq)
{
  int frag,fd,rate;

  rate=*freq;

  fd=open("/dev/dsp",O_WRONLY,0);

  if(fd==-1)
  {
    perror("init_dsp: unable to open /dev/dsp");
    exit(EXIT_FAILURE);
  }

  ioctl(fd,SNDCTL_DSP_SETFMT,&format);

  ioctl(fd,SNDCTL_DSP_STEREO,&stereo);

  ioctl(fd,SNDCTL_DSP_SPEED,&rate);

  ioctl(fd,SOUND_PCM_READ_RATE,&rate);

  *freq=(float)rate;

  printf("init_dsp: sample frequency is %f\n",*freq);

  frag=0x00080009;
  ioctl(fd,SNDCTL_DSP_SETFRAGMENT,&frag);

  return fd;
}
