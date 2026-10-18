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
#include<math.h>
#include<unistd.h>
#include<signal.h>
#include<sys/soundcard.h>

#include"synth.h"
#include"my_types.h"

#define FREQMAX 0.825                /* filter only stable to this frequency */

extern int pd[2];

static void sig_usr(int);

void synth(value *vptr,midi_info *mptr)
{
  int fd;

  float sample_frequency=SAMPLE_RATE;

  register int i;

  unsigned char note,prev_note=0;

  signed short sample[BUFF_SIZE],silence[BUFF_SIZE]={0};

  float deltat,deltat2;

  float fund_pitch,prev_pitch,pitch[128];

  float lfo,omega3,lfo_pos,lfo_amount_o,lfo_amount_f;
  unsigned char lfo_waveform;

  float eg1,eg1_rate_level[3]={0.},eg1_one_rate[3]={0.};
  float eg1_amount_o;
  unsigned char eg1_phase;

  float eg2,eg2_rate_level[3]={0.},eg2_one_rate[3]={0.};
  float eg2_amount_o;
  unsigned char eg2_phase;

  float osc1,omega1,osc1_pw;
  float osc1_pos=0.;
  unsigned char osc1_waveform;

  unsigned char osc_sync,sync_flag1=0,sync_flag2=0,sync_flag3=0;

  float osc2,omega2,omega2_,osc2_pw;
  float osc2_pos=0.;
  unsigned char osc2_waveform;

  float balance1,balance2;

  float freq,freqcut,freqkey,freqeg1,freqeg2,qres;
  float highpass,delay1=0.,delay2=0.,delay3=0.,delay4=0.;
  unsigned char pole4;

  float input,output;

  float vol_out;

  float oscillator(float *,float,float,unsigned char,float,unsigned char *);
  float volume(float);
  void pitch_init(float *);
  void volume_init(void);
  void init_waveforms(void);
  int init_dsp(int,int,float *);

  if(signal(SIGUSR1,sig_usr) == SIG_ERR)
  {
    perror("synth: cannot catch SIGUSR1");
    exit(EXIT_FAILURE);
  }

  fd=init_dsp(AFMT_S16_LE,0,&sample_frequency);  /* initialise output device */

  pitch_init(pitch);                        /* initialise the look-up tables */
  init_waveforms();
  volume_init();

  deltat=deltat2=1./sample_frequency;

  printf("\nXsynth - a real-time software synthesizer\n");

  read_notes:                             /* infinite synth loop starts here */
                         /* gotos seem to be much faster than while(1) loops */

    for(i=96; i>20; --i)     /* determine note played, highest note priority */
    {
      if(mptr->note_table[i])
      {
        if(prev_note)                                /* previously held note */
	{
          fund_pitch=vptr->cont[8]*pitch[i]
                     +(1.-vptr->cont[8])*prev_pitch;           /* portamento */

          if(i != prev_note)      /* if note has changed then re-trigger egs */
          {
            prev_note=i;
            eg1_phase=eg2_phase=0;
          }
        }
        else                                      /* no previously held note */
	{
          prev_note=i;
          fund_pitch=pitch[i];
          eg1_phase=eg2_phase=0;
        }
        prev_pitch=fund_pitch;
        fund_pitch*=mptr->pitch_wheel;      /* modify pitch after portamento */
        freq=2.*PI/sample_frequency*fund_pitch*mptr->mod_wheel;
        freqkey=freq*vptr->cont[21];
        freqeg1=freq*vptr->cont[14];
        freqeg2=freq*vptr->cont[20];
        goto make_sound;
      }
    }

    prev_note=0;              /* if no note being held then reset portamento */

    if(eg1 > 1.0e-2)            /* allow sound to complete its release phase */
    {
      eg1_phase=eg2_phase=2;
      goto make_sound;
    }

    delay1=delay2=0.;             /* no sound, so reset filter delay storage */
    delay3=delay4=0.;

    write(fd,silence,BUFF_SIZE*sizeof(short int));

    goto read_notes;

    make_sound: omega1=vptr->cont[0]*fund_pitch;  /* read from shared memory */
                osc1_waveform=vptr->dete[0];
                osc1_pw=vptr->cont[1];
                omega2=vptr->cont[2]*fund_pitch;
                osc2_waveform=vptr->dete[1];
                osc2_pw=vptr->cont[3];
                osc_sync=vptr->on_off[0];
                omega3=vptr->cont[4];
                lfo_waveform=vptr->dete[2];
                lfo_amount_o=vptr->cont[5];
                lfo_amount_f=vptr->cont[6];
                eg1_rate_level[0]=vptr->cont[9];
                eg1_one_rate[0]=1.-vptr->cont[9];
                eg1_rate_level[1]=vptr->cont[10]*vptr->cont[11];
                eg1_one_rate[1]=1.-vptr->cont[10];
                eg1_one_rate[2]=1.-vptr->cont[12];
                eg1_amount_o=vptr->cont[13];
                eg2_rate_level[0]=vptr->cont[15];
                eg2_one_rate[0]=1.-vptr->cont[15];
                eg2_rate_level[1]=vptr->cont[16]*vptr->cont[17];
                eg2_one_rate[1]=1.-vptr->cont[16];
                eg2_one_rate[2]=1.-vptr->cont[18];
                eg2_amount_o=vptr->cont[19];
                qres=vptr->cont[22];
                pole4=vptr->on_off[1];
                balance1=1.-vptr->cont[7];
                balance2=vptr->cont[7];
                vol_out=volume(vptr->cont[23]);

    for(i=0; i<BUFF_SIZE; ++i)                /* loop to calculate the sound */
    {

/*    LFO section */

      lfo=oscillator(&lfo_pos,omega3,deltat,lfo_waveform,0.25,&sync_flag3);

/*    EG1 section */

      eg1=eg1_rate_level[eg1_phase]+eg1_one_rate[eg1_phase]*eg1;

      if(!eg1_phase && eg1>0.99)eg1_phase=1;    /* flip from attack to decay */

/*    EG2 section */

      eg2=eg2_rate_level[eg2_phase]+eg2_one_rate[eg2_phase]*eg2;

      if(!eg2_phase && eg2>0.99)eg2_phase=1;    /* flip from attack to decay */

/*    VCO 1 section */

      osc1=oscillator(&osc1_pos,omega1,deltat,
                      osc1_waveform,osc1_pw,&sync_flag1);

/*    oscillator sync control */

      if(osc_sync&sync_flag1)
      {
        sync_flag1=0;
        osc2_pos=0.;
        deltat2=osc1_pos/omega1;
      }
      else
        deltat2=deltat;

/*    VCO 2 section */

      omega2_=omega2
              *(1.+eg1*eg1_amount_o)
              *(1.+eg2*eg2_amount_o)
              *(1.+lfo*lfo_amount_o);

      osc2=oscillator(&osc2_pos,omega2_,deltat2,
                      osc2_waveform,osc2_pw,&sync_flag2);

/*    cross modulation */

/*    mixer section */

      input=balance1*osc1+balance2*osc2;

/*    VCF section - Hal Chamberlin's state variable filter */

      freqcut=(freqkey+freqeg1*eg1+freqeg2*eg2)*(1.+lfo*lfo_amount_f);

      if(freqcut > FREQMAX)freqcut=FREQMAX;

      delay2=delay2+freqcut*delay1;             /* delay2/4 = lowpass output */
      highpass=input-delay2-qres*delay1;
      delay1=freqcut*highpass+delay1;          /* delay1/3 = bandpass output */
      output=delay2;

      if(pole4)   /* above gives 12db per octave, this gives 24db per octave */
      {
        delay4=delay4+freqcut*delay3;
        highpass=output-delay4-qres*delay3;
        delay3=freqcut*highpass+delay3;
        output=delay4;
      }

/*    VCA section */

      output*=eg1*vol_out;

      sample[i]=(signed short) 0x7fff*output;    /* update the sample buffer */
    }

    write(fd,sample,BUFF_SIZE*sizeof(short int));          /* make the sound */

    goto read_notes;
}

/* function to shut down synth engine */

static void sig_usr(int signo)
{
  if(signo == SIGUSR1)
  {
    printf("synth: received SIGUSR1 - shutting down\n");
    exit(EXIT_SUCCESS);
  }

  return;
}
