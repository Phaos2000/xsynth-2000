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
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<sys/mman.h>
#include<unistd.h>
#include<fcntl.h>
#include<signal.h>
#include<sched.h>

#include"my_types.h"

main()
{
  int shmid1,shmid2;
  void *shmptr;
  value *vptr;
  midi_info *mptr;
  pid_t pid1,pid2;
  struct sched_param scheduler;

  void user_interface(value *,midi_info *);
  void synth(value *,midi_info *);
  void midi(midi_info *);

  if((shmid1=shmget(IPC_PRIVATE,sizeof(value),SHM_R | SHM_W)) < 0)
  {
    perror("main: shmget error");
    exit(EXIT_FAILURE);
  }

  if((shmptr=shmat(shmid1,0,0)) == (void *) -1)
  {
    perror("main: shmat error");
    shmctl(shmid1,IPC_RMID,0);
    exit(EXIT_FAILURE);
  }

  vptr=(value *)shmptr;

  if((shmid2=shmget(IPC_PRIVATE,sizeof(midi_info),SHM_R | SHM_W)) < 0)
  {
    perror("main: shmget error");
    shmdt((void *)vptr);
    shmctl(shmid1,IPC_RMID,0);
    exit(EXIT_FAILURE);
  }

  if((shmptr=shmat(shmid2,0,0)) == (void *) -1)
  {
    perror("main: shmat error");
    shmdt((void *)vptr);
    shmctl(shmid1,IPC_RMID,0);
    shmctl(shmid2,IPC_RMID,0);
    exit(EXIT_FAILURE);
  }

  mptr=(midi_info *)shmptr;

  if((pid1=fork())<0)
  {
    perror("main: fork error for first child");
    shmdt((void *)vptr);
    shmdt((void *)mptr);
    shmctl(shmid1,IPC_RMID,0);
    shmctl(shmid2,IPC_RMID,0);
    exit(EXIT_FAILURE);
  }

  if(pid1 == 0)                     /* first child process, the synth engine */
  {
    if(mlockall(MCL_CURRENT | MCL_FUTURE))
      perror("main: memory lock has failed");

    scheduler.sched_priority=99;

    if(sched_setscheduler(0,SCHED_FIFO,&scheduler)<0)
      perror("main: problem setting process scheduling");

    sched_getparam(0,&scheduler);

    printf("main: scheduling priority is %d\n",scheduler.sched_priority);

    synth(vptr,mptr);
  }
  else
  {
    if((pid2=fork())<0)
    {
      perror("main: fork error for second child");
      kill(pid1,SIGUSR1);
      waitpid(pid1,NULL,0);
      shmdt((void *)vptr);
      shmdt((void *)mptr);
      shmctl(shmid1,IPC_RMID,0);
      shmctl(shmid2,IPC_RMID,0);
      exit(EXIT_FAILURE);
    }

    if(pid2 == 0)               /* second child process, the keyboard server */
    {
      midi(mptr);
    }
    else         /* the parent process, control passes to the user_interface */
    {
      user_interface(vptr,mptr);

      kill(pid2,SIGUSR1);                        /* kill the keyboard server */
      waitpid(pid2,NULL,0);

      kill(pid1,SIGUSR1);                           /* kill the synth engine */
      waitpid(pid1,NULL,0);

      shmdt((void *)vptr);     /* detach shared memory blocks and remove ids */
      shmdt((void *)mptr);
      shmctl(shmid1,IPC_RMID,0);
      shmctl(shmid2,IPC_RMID,0);

      exit(EXIT_SUCCESS);
    }
  }
}
