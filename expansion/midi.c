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

#include<X11/Xlib.h>
#include<X11/Xutil.h>
#include<X11/Xos.h>

#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<signal.h>
#include<math.h>
#include<sys/ioctl.h>

#include"my_types.h"

#define LN2 0.69314718

static int fd;

static void sig_usr(int);

void midi(midi_info *mptr)
{
  int i;
  unsigned char midi_data,masked_data,current_status=0;
  unsigned char midi_note,midi_velocity;
  unsigned char data1,data2;

  void init_midi(void);
  unsigned char get_data(void);

  if(signal(SIGUSR1,sig_usr) == SIG_ERR)
  {
    perror("midi: cannot catch SIGUSR1");
    exit(EXIT_FAILURE);
  }

  init_midi();

  /* initialise the MIDI information for the synth engine */

  for(i=0; i<128; ++i)mptr->note_table[i]=0;
  mptr->pitch_wheel=1.0;
  mptr->mod_wheel=1.0;

  while(1)
  {
    midi_data=get_data();

    beginning: masked_data=midi_data&0xf0;

    if(masked_data == 0xf0)
    {
      switch(midi_data)
      {
      case 0xf0:                                    /* system exclusive dump */
        while(((midi_data=get_data())&0x80) == 0);
        if(midi_data != 0xf7)                  /* no sys ex termination byte */
          goto beginning;
        break;

      case 0xf1:                                                /* undefined */
        break;

      case 0xf2:                                    /* song position pointer */
        data1=get_data();

        if(data1&0x80)                             /* then there is an error */
        {
          printf("error: midi receive problem\n");
          midi_data=data1;
          goto beginning;
        }

        data2=get_data();

        if(data2&0x80)                             /* then there is an error */
        {
          printf("error: midi receive problem\n");
          midi_data=data2;
          goto beginning;
        }

        break;

      case 0xf3:                                              /* song select */
        data1=get_data();

        if(data1&0x80)                             /* then there is an error */
        {
          printf("error: midi receive problem\n");
          midi_data=data1;
          goto beginning;
        }

        break;

      case 0xf4:                                                /* undefined */
        break;

      case 0xf5:                                                /* undefined */
        break;

      case 0xf6:                                             /* tune request */
        break;
      }
    }
    else
    {
      if(midi_data&0x80)
      {
        current_status=masked_data;
        data1=get_data();

        if(data1&0x80)                             /* then there is an error */
        {
          printf("error: midi receive problem\n");
          midi_data=data1;
          goto beginning;
        }
      }
      else
      {
        data1=midi_data;
      }

      switch(current_status)
      {
      case 0x00:                           /* current status byte is not set */
        printf("error: current status byte is not set\n");
        break;

      case 0x80:                                           /* note off event */
        midi_note=data1;
        midi_velocity=get_data();

        if(midi_velocity&0x80)                     /* then there is an error */
        {
          printf("error: midi receive problem\n");
          midi_data=midi_velocity;
          goto beginning;
        }

        mptr->note_table[midi_note]=0;
        break;

      case 0x90:                                            /* note on event */
        midi_note=data1;
        midi_velocity=get_data();

        if(midi_velocity&0x80)                     /* then there is an error */
        {
          printf("error: midi receive problem\n");
          midi_data=midi_velocity;
          goto beginning;
        }

        mptr->note_table[midi_note]=midi_velocity;
        break;

      case 0xa0:                     /* polyphonic key pressure - aftertouch */
        midi_note=data1;
        data2=get_data();

        if(data2&0x80)                             /* then there is an error */
        {
          printf("error: midi receive problem\n");
          midi_data=data2;
          goto beginning;
        }

        break;

      case 0xb0:                                           /* control change */
        data2=get_data();

        if(data2&0x80)                             /* then there is an error */
        {
          printf("error: midi receive problem\n");
          midi_data=data2;
          goto beginning;
        }

        if(data1 == 0x01)
          mptr->mod_wheel=1.-(float)data2/0x7f;
        break;

      case 0xc0:                                           /* program change */
        break;

      case 0xd0:                            /* channel pressure - aftertouch */
        break;

      case 0xe0:                                      /* pitch bend - change */
        data2=get_data();

        if(data2&0x80)                             /* then there is an error */
        {
          printf("error: midi receive problem\n");
          midi_data=data2;
          goto beginning;
        }

        mptr->pitch_wheel=exp(((float)(data1+0x80*data2)/0x2000-1.)*LN2);
        break;
      }
    }
  }
}

/* function to read MIDI data whilst ignoring real-time messages */

unsigned char get_data(void)
{
  unsigned char midi_data;

  while(1)
  {
    read(fd,&midi_data,sizeof(unsigned char));

    /*    printf("get_data: MIDI data byte %x\n",midi_data); */

    if((midi_data&0xf8) != 0xf8)                  /* dump real-time messages */
      return midi_data;
  }
}

/* function to initialise /dev/midi */

void init_midi(void)
{
  fd=open("/dev/midi",O_RDONLY,0);

  if(fd == -1)
  {
    perror("init_midi: unable to open /dev/midi");
    exit(EXIT_FAILURE);
  }

  return;
}

/* function to shut down keyboard server */

static void sig_usr(int signo)
{
  if(signo == SIGUSR1)
  {
    printf("midi: received SIGUSR1 - shutting down\n");
    exit(EXIT_SUCCESS);
  }

  return;
}
