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
#include<stddef.h>
#include<string.h>
#include<stdlib.h>
#include<math.h>

#include"synth.h"
#include"my_types.h"
#include"xsynth_bitmap"

static Display *display;
static int screen;
static unsigned long fore_col,back_col;

#define SMALL 1
#define OK 0

#define CKNOB 1
#define DKNOB 2
#define BUTTON 4

#define NDETENTS 6

#define RATIO (44100/SAMPLE_RATE)     /* compensate controls for SAMPLE_RATE */

void user_interface(value *vptr,midi_info *mptr)
{
  Window win;
  unsigned int width=500,height=525;
  int i,x,y,px=0,py=0;
  unsigned int border_width=4;
  unsigned int display_width,display_height;
  char *window_name="Xsynth";
  char *kwindow_name="";
  char *icon_name="Xsynth";
  char *kicon_name="";
  Pixmap icon_pixmap;
  XSizeHints size_hints;
  XEvent report;
  GC gc,gcd;
  XFontStruct *font_info;
  char *display_name=NULL;
  int window_size=0;
  XSetWindowAttributes attributes;

  void set_cols(void);                                /* function prototypes */
  void draw_pointer(GC,control *);
  void draw_button(GC,GC,control *,unsigned char);
  void draw_knob(Window,GC,control *);
  void draw_scale(Window,GC,XFontStruct *,control *);
  void draw_scaled(Window,GC,XFontStruct *,control *);
  void draw_labels(Window,GC,XFontStruct *,control *,int);
  void draw_panel(Window,GC);
  void move_pointer(control *,GC,GC,int,int,float *);
  void move_detent(control *,GC,GC,int,int,unsigned char *);
  void init_values(value *,control *,control *);
  void load_patch(GC,GC,value *,control *,control *,control *);
  void save_patch(value *,control *,control *);

  control cknobs[NCKNOBS]=
  {
    /* VCO 1 */
    {1,0,0,35,30,30,30,15.,5./4.*PI,1,1.,2.,2.,"pitch"},
    {1,0,0,35,170,30,30,15.,1./2.*PI,0,0.,1.,0.,"pulsewidth"},

    /* VCO 2 */
    {1,0,0,35,270,30,30,15.,5./4.*PI,1,1.,2.,2.,"pitch"},
    {1,0,0,35,410,30,30,15.,1./2.*PI,0,0.,1.,0.,"pulsewidth"},

    /* LFO */
    {1,0,0,135,30,30,30,15.,5./4.*PI,1,0.1,10.,2.,"lfo frequency"},
    {1,0,0,135,170,30,30,15.,5./4.*PI,0,0.,1.,0.,"pitch depth"},
    {1,0,0,135,240,30,30,15.,5./4.*PI,0,0.,1.,0.,"filter depth"},

    /* BALANCE */
    {1,0,0,135,340,30,30,15.,1./2.*PI,0,0.,1.,0.,"balance"},

    /* PORTAMENTO */
    {1,0,0,135,440,30,30,15.,5./4.*PI,1,1.,0.01*RATIO,1.,"glide time"},

    /* EG 1 */
    {1,0,0,235,30,30,30,15.,5./4.*PI,1,0.1*RATIO,10.,-4.,"attack time"},
    {1,0,0,235,100,30,30,15.,5./4.*PI,1,0.1*RATIO,10.,-4.,"decay time"},
    {1,0,0,235,170,30,30,15.,-1./4.*PI,0,0.,1.,0.,"sustain level"},
    {1,0,0,235,240,30,30,15.,5./4.*PI,1,0.1*RATIO,10.,-4.,"release time"},
    {1,0,0,235,310,30,30,15.,5./4.*PI,0,0.,1.,0.,"pitch depth"},
    {1,0,0,235,380,30,30,15.,5./4.*PI,0,0.,50.,0.,"filter depth"},

    /* EG 2 */
    {1,0,0,335,30,30,30,15.,5./4.*PI,1,0.1*RATIO,10.,-4.,"attack time"},
    {1,0,0,335,100,30,30,15.,5./4.*PI,1,0.1*RATIO,10.,-4.,"decay time"},
    {1,0,0,335,170,30,30,15.,-1./4.*PI,0,0.,1.,0.,"sustain level"},
    {1,0,0,335,240,30,30,15.,5./4.*PI,1,0.1*RATIO,10.,-4.,"release time"},
    {1,0,0,335,310,30,30,15.,5./4.*PI,0,0.,1.,0.,"pitch depth"},
    {1,0,0,335,380,30,30,15.,5./4.*PI,0,0.,50.,0.,"filter depth"},

    /* VCF */
    {1,0,0,435,30,30,30,15.,-1./4.*PI,0,0.,50.,0.,"cutoff"},
    {1,0,0,435,100,30,30,15.,5./4.*PI,0,0.,-1.995,2.,"resonance"},

    /* VOLUME */
    {1,0,0,435,270,30,30,15.,1./2.*PI,0,0.,1.,0.,"volume"}
  };

  control dknobs[NDKNOBS]=
  {
    {2,0,0,35,100,30,30,15.,5./4.*PI,0,0.,0.,0.,"waveform"},
    {2,0,0,35,340,30,30,15.,5./4.*PI,0,0.,0.,0.,"waveform"},
    {2,0,0,135,100,30,30,15.,5./4.*PI,0,0.,0.,0.,"waveform"}
  };

  control buttons[NBUTTONS]=
  {
    {4,0,3,40,480,20,15,0.,0.,0,0.,0.,0.,"oscillator sync"},
    {4,0,3,440,170,20,15,0.,0.,0,0.,0.,0.,"12-24 db/oct"},
    {4,0,3,240,480,20,15,0.,0.,0,0.,0.,0.,"LOAD PATCH"},
    {4,0,3,340,480,20,15,0.,0.,0,0.,0.,0.,"SAVE PATCH"},
    {4,0,3,440,370,20,15,0.,0.,0,0.,0.,0.,"POWER"},
  };

  init_values(vptr,cknobs,dknobs);

/* connect to the X server */

  if((display=XOpenDisplay(display_name))==NULL)
  {
    perror("user_interface: cannot connect to X server");
    return;
  }

  screen=DefaultScreen(display);

  display_width=DisplayWidth(display,screen);
  display_height=DisplayHeight(display,screen);

/* set the colours */

  set_cols();

/* create the background window */

  win=XCreateSimpleWindow(display,RootWindow(display,screen),px,py,width,
                          height,border_width,fore_col,back_col);

  icon_pixmap=XCreateBitmapFromData(display,win,icon_bitmap_bits,
                                    icon_bitmap_width,icon_bitmap_height);

  size_hints.flags=PPosition | PSize | PMinSize | PMaxSize;
  size_hints.x=px;
  size_hints.y=py;
  size_hints.width=width;
  size_hints.height=height;
  size_hints.min_width=width;
  size_hints.min_height=height;
  size_hints.max_width=width;
  size_hints.max_height=height;

  XSetStandardProperties(display,win,window_name,icon_name,icon_pixmap,
                         NULL,0, &size_hints);

  XSelectInput(display,win,ExposureMask | StructureNotifyMask);

/* create the control windows */

  attributes.do_not_propagate_mask=ButtonPressMask;

  for(i=0;i<NCKNOBS;++i)
  {
    cknobs[i].win=XCreateSimpleWindow(display,win,
                                      cknobs[i].x-cknobs[i].border_width,
                                      cknobs[i].y-cknobs[i].border_width,
                                      cknobs[i].width,cknobs[i].height,
                                      cknobs[i].border_width,
                                      fore_col,back_col);

    XChangeWindowAttributes(display,cknobs[i].win,CWDontPropagate,&attributes);

    XSelectInput(display,cknobs[i].win,
                 ExposureMask | ButtonPressMask |
                 ButtonMotionMask | ButtonReleaseMask);
  }

  for(i=0;i<NDKNOBS;++i)
  {
    dknobs[i].win=XCreateSimpleWindow(display,win,
                                      dknobs[i].x-dknobs[i].border_width,
                                      dknobs[i].y-dknobs[i].border_width,
                                      dknobs[i].width,dknobs[i].height,
                                      dknobs[i].border_width,
                                      fore_col,back_col);

    XChangeWindowAttributes(display,dknobs[i].win,CWDontPropagate,&attributes);

    XSelectInput(display,dknobs[i].win,
                 ExposureMask | ButtonPressMask |
                 ButtonMotionMask | ButtonReleaseMask);
  }

  for(i=0;i<NBUTTONS;++i)
  {
    buttons[i].win=XCreateSimpleWindow(display,win,
                                       buttons[i].x-buttons[i].border_width,
                                       buttons[i].y-buttons[i].border_width,
                                       buttons[i].width,buttons[i].height,
                                       buttons[i].border_width,
                                       fore_col,back_col);

    XChangeWindowAttributes(display,buttons[i].win,
                            CWDontPropagate,&attributes);

    XSelectInput(display,buttons[i].win,
                 ExposureMask | ButtonPressMask | ButtonReleaseMask);
  }

/* load the fonts */

  load_font(&font_info);

/* create the graphics contexts */

  get_GC(win,&gc,font_info,fore_col);

  get_GC(win,&gcd,font_info,back_col);

/* map the main window */

  XMapWindow(display,win);

/* map the control windows */

  for(i=0;i<NCKNOBS;++i)
  {
    XMapWindow(display,cknobs[i].win);
  }

  for(i=0;i<NDKNOBS;++i)
  {
    XMapWindow(display,dknobs[i].win);
  }

  for(i=0;i<NBUTTONS;++i)
  {
    XMapWindow(display,buttons[i].win);
  }

  while(1)                         /* start of infinite event gathering loop */
  {
    XNextEvent(display,&report);

    switch(report.type)
    {
    case Expose:
      while(XCheckTypedEvent(display,Expose,&report));
      if(window_size==SMALL)
        TooSmall(win,gc,font_info);
      else
      {
        draw_panel(win,gc);

        for(i=0;i<NCKNOBS;++i)
	{
          draw_pointer(gc,&cknobs[i]);
          draw_knob(win,gc,&cknobs[i]);
          draw_scale(win,gc,font_info,&cknobs[i]);
        }
        draw_labels(win,gc,font_info,cknobs,NCKNOBS);

        for(i=0;i<NDKNOBS;++i)
	{
          draw_pointer(gc,&dknobs[i]);
          draw_knob(win,gc,&dknobs[i]);
          draw_scaled(win,gc,font_info,&dknobs[i]);
	}
        draw_labels(win,gc,font_info,dknobs,NDKNOBS);

        for(i=0;i<NBUTTONS;++i)
	{
          draw_button(gc,gcd,&buttons[i],vptr->on_off[i]);
        }
        draw_labels(win,gc,font_info,buttons,NBUTTONS);
      }
    break;

    case ConfigureNotify:
      width=report.xconfigure.width;
      height=report.xconfigure.height;
      if((width<size_hints.min_width) || (height<size_hints.min_height))
        window_size=SMALL;
      else
        window_size=OK;
      break;

    case MotionNotify:
      for(i=NCKNOBS-1;i>-1;--i)
      {
        if(cknobs[i].win == report.xany.window)break;
      }

      if(i > -1)
      {
        move_pointer(&cknobs[i],gc,gcd,
                     report.xmotion.x,report.xmotion.y,&(vptr->cont[i]));
        break;
      }

      for(i=NDKNOBS-1;i>-1;--i)
      {
        if(dknobs[i].win == report.xany.window)break;
      }

      if(i > -1)
      {
        move_detent(&dknobs[i],gc,gcd,
                    report.xmotion.x,report.xmotion.y,&(vptr->dete[i]));
        break;
      }
      else
        break;

    case ButtonPress:
      for(i=NCKNOBS-1;i>-1;--i)
      {
        if(cknobs[i].win == report.xany.window)break;
      }

      if(i > -1)
      {
        move_pointer(&cknobs[i],gc,gcd,
                     report.xmotion.x,report.xmotion.y,&(vptr->cont[i]));
        break;
      }

      for(i=NDKNOBS-1;i>-1;--i)
      {
        if(dknobs[i].win == report.xany.window)break;
      }

      if(i > -1)
      {
        move_detent(&dknobs[i],gc,gcd,
                    report.xmotion.x,report.xmotion.y,&(vptr->dete[i]));
        break;
      }

      for(i=NBUTTONS-1;i>-1;--i)
      {
        if(buttons[i].win == report.xany.window)break;
      }

      if(i > -1)
      {
        vptr->on_off[i]=!vptr->on_off[i];
        draw_button(gc,gcd,&buttons[i],vptr->on_off[i]);
        break;
      }
      else
        break;

    case ButtonRelease:
      for(i=NBUTTONS-1;i>-1;--i)
      {
        if(buttons[i].win == report.xany.window)break;
      }

      switch(i)
      {
      case NBUTTONS-3:            /* this button number is always load patch */
	load_patch(gc,gcd,vptr,cknobs,dknobs,buttons);
        vptr->on_off[i]=!vptr->on_off[i];
        draw_button(gc,gcd,&buttons[i],vptr->on_off[i]);
        break;

      case NBUTTONS-2:            /* this button number is always save patch */
	save_patch(vptr,cknobs,dknobs);
        vptr->on_off[i]=!vptr->on_off[i];
        draw_button(gc,gcd,&buttons[i],vptr->on_off[i]);
        break;

      case NBUTTONS-1:          /* this button number is always program exit */
        printf("exiting Xsynth\n");
        XUnloadFont(display,font_info->fid);
        XFreeGC(display,gc);
        XCloseDisplay(display);
        return;

      default:             /* normally do nothing when mouse button released */
        break;
      }

    case KeyPress:
      break;

    default:
      break;
    }
  }
}

void set_cols(void)
{
  XColor colour,colour_exact;
  Colormap cmap;

  char *col1="orange";
  char *col2="black";

  cmap=DefaultColormap(display,screen);

  if(!XAllocNamedColor(display,cmap,col1,&colour,&colour_exact))
  {
    printf("user_interface: unable to get background colour\n");
    back_col=WhitePixel(display,screen);
  }
  else
    back_col=colour.pixel;

  if(!XAllocNamedColor(display,cmap,col2,&colour,&colour_exact))
  {
    printf("user_interface: unable to get foreground colour\n");
    fore_col=BlackPixel(display,screen);
  }
  else
    fore_col=colour.pixel;
}

void move_pointer(control *kp,GC gc,GC gcd,int x,int y,float *value)
{
  int xc,yc;
  float cval;
  void draw_pointer(GC,control *);

  xc=kp->width/2;
  yc=kp->height/2;

  draw_pointer(gcd,kp);

  kp->angle=atan((float)(yc-y)/(float)(x-xc));
  if(x-xc<0)kp->angle=kp->angle+PI;
  if(kp->angle>5./4.*PI)kp->angle=5./4.*PI;
  if(kp->angle<-1./4.*PI)kp->angle=-1./4.*PI;

  cval=(5./4.*PI-kp->angle)/(3./2.*PI);

  if(kp->lin_log)
    *value=kp->a*exp(kp->c*cval*log(kp->b));                  /* logarithmic */
  else
    *value=(kp->a*cval+kp->b)*cval+kp->c;             /* quadratic or linear */

  draw_pointer(gc,kp);
}

void move_detent(control *kp,GC gc,GC gcd,int x,int y,unsigned char *value)
{
  int i,xc,yc;
  float dangles[NDETENTS]={5./4.*PI,19./20.*PI,13./20.*PI,
                           7./20.*PI,1./20.*PI,7./4.*PI};
  unsigned char values[NDETENTS]={1,2,4,8,16,32};
  float diff,min_diff;
  int dpos=0;
  void draw_pointer(GC,control *);

  xc=kp->width/2;
  yc=kp->height/2;

  draw_pointer(gcd,kp);
  kp->angle=atan((float)(yc-y)/(float)(x-xc));
  if(x-xc<0)kp->angle=kp->angle+PI;
  if(kp->angle<0)kp->angle=kp->angle+2.*PI;
  min_diff=(kp->angle-dangles[0])*(kp->angle-dangles[0]);
  for(i=1;i<NDETENTS;++i)
  {
    diff=(kp->angle-dangles[i])*(kp->angle-dangles[i]);
    if(diff<min_diff)
    {
      min_diff=diff;
      dpos=i;
    }
  }
  kp->angle=dangles[dpos];
  *value=values[dpos];
  draw_pointer(gc,kp);
}

get_GC(Window win,GC *gc,XFontStruct *font_info,int pixel)
{
  unsigned long valuemask=GCSubwindowMode;
  XGCValues values;
  unsigned int line_width=3;
  int line_style=LineSolid;
  int cap_style=CapButt;
  int join_style=JoinMiter;

  values.subwindow_mode=IncludeInferiors;

  *gc=XCreateGC(display,win,valuemask,&values);

  XSetFont(display,*gc,font_info->fid);

  XSetForeground(display,*gc,pixel);

  XSetLineAttributes(display,*gc,line_width,line_style,cap_style,join_style);
}

load_font(XFontStruct **font_info)
{
  char *fontname="6x10";

  if((*font_info=XLoadQueryFont(display,fontname))==NULL)
  {
    fprintf(stderr,"user_interface: cannot open %s font\n",fontname);
    return;
  }
}

void draw_pointer(GC gc,control *kp)
{
  int xo,yo,xr,yr,xi,yi;
  float r,theta;

  xo=kp->width/2;
  yo=kp->height/2;
  r=kp->radius*0.8;
  theta=kp->angle;

  xr=(int)(r*cos(theta));
  yr=(int)(r*sin(theta));

  xi=xr*0.5;
  yi=yr*0.5;

  xr=xr+xo;
  yr=yo-yr;

  xi=xi+xo;
  yi=yo-yi;

  XDrawLine(display,kp->win,gc,xi,yi,xr,yr);
}

void draw_button(GC gc,GC gcd,control *bp,unsigned char on_off)
{
  if(on_off)
    XFillRectangle(display,bp->win,gc,0,0,bp->width,bp->height);
  else
    XFillRectangle(display,bp->win,gcd,0,0,bp->width,bp->height);
}

void draw_knob(Window win,GC gc,control *kp)
{
  int angle1=0,angle2=360*64;

  XDrawArc(display,win,gc,kp->x,kp->y,kp->width,kp->height,angle1,angle2);
}

void draw_scale(Window win,GC gc,XFontStruct *font_info,control *kp)
{
  int i,xc,yc,xo,yo;

  char *scale[11]={"0","1","2","3","4","5","6","7","8","9","10"};

  float theta,r;

  xo=kp->x+kp->width/2;
  yo=kp->y+kp->height/2;

  r=kp->radius*1.75;

  for(i=0;i<11;++i)
  {
    theta=5./4.*PI-(float)i/10.*3./2.*PI;
    xc=(int)r*cos(theta)+xo-2;
    yc=yo-(int)r*sin(theta)+4;

    XDrawString(display,win,gc,xc,yc,scale[i],strlen(scale[i]));
  }
}

void draw_scaled(Window win,GC gc,XFontStruct *font_info,control *kp)
{
  int i,xc,yc,xo,yo;

  char *scale[6]={"s","t","u","d","q","p"};

  float theta,r;

  xo=kp->x+kp->width/2;
  yo=kp->y+kp->height/2;

  r=kp->radius*1.75;

  for(i=0;i<6;++i)
  {
    theta=5./4.*PI-(float)i/5.*3./2.*PI;
    xc=(int)r*cos(theta)+xo-2;
    yc=yo-(int)r*sin(theta)+4;

    XDrawString(display,win,gc,xc,yc,scale[i],strlen(scale[i]));
  }
}

void draw_labels(Window win,GC gc,XFontStruct *font_info,control *cp,int n)
{
  int i,w,x,y;

  for(i=0;i<n;++i)
  {
    w=XTextWidth(font_info,cp[i].label,strlen(cp[i].label));
    x=cp[i].x+(cp[i].width-w)/2;
    y=cp[i].y+cp[i].height+20;
    XDrawString(display,win,gc,x,y,cp[i].label,strlen(cp[i].label));
  }
}

void draw_panel(Window win,GC gc)
{
  int i;

  int x1[8]={0,101,201,401,100,200,300,400};
  int x2[8]={100,200,500,500,100,200,300,400};
  int y1[8]={240,310,450,240,0,0,0,0};
  int y2[8]={240,310,450,240,525,525,450,450};

  int xsect[10]={5,5,105,105,205,305,405,405,105,415};
  int ysect[10]={10,250,10,320,10,10,10,250,420,515};
  char *sect[10]={"VCO1","VCO2","LFO","MIXER","EG1 (VCA)",
                      "EG2","VCF","VOLUME","PORTAMENTO","Xsynth 1.0.2"};

  for(i=0;i<8;++i)
  {
    XDrawLine(display,win,gc,x1[i],y1[i],x2[i],y2[i]);
  }

  for(i=0;i<10;++i)
  {
    XDrawString(display,win,gc,xsect[i],ysect[i],sect[i],strlen(sect[i]));
  }
}

void init_values(value *vp,control *cknobs,control *dknobs)
{
  int i;
  float cval;

  for(i=0;i<NCKNOBS;++i)
  {
    cval=(5./4.*PI-cknobs[i].angle)/(3./2.*PI);
    if(cknobs[i].lin_log)
      vp->cont[i]=cknobs[i].a*pow(cknobs[i].b,cknobs[i].c*cval);
    else
      vp->cont[i]=(cknobs[i].a*cval+cknobs[i].b)*cval+cknobs[i].c;
  }

  for(i=0;i<NDKNOBS;++i)
  {
    vp->dete[i]=1;
  }

  for(i=0;i<NBUTTONS-1;++i)
  {
    vp->on_off[i]=0;
  }

  vp->on_off[NBUTTONS-1]=1;
}

void load_patch(GC gc,GC gcd,value *vp,
                control *cknobs,control *dknobs,control *buttons)
{
  FILE *fp;
  char name[108];
  const char *suffix=".Xpatch";
  int i;

  void draw_pointer(GC,control *);
  void draw_button(GC,GC,control *,unsigned char);

  printf("\nAvailable patches are:\n");

  system("ls *.Xpatch");

  printf("\nName of patch to be loaded (without the .Xpatch)?\n");

  scanf("%100s",name);

  strcat(name,suffix);

  printf("\nAttempting to load patch information from file %s...\n",name);

  if((fp=fopen(name,"rb")) == NULL)
    perror("\nload_patch");
  else
  {

 /*
  * Note: with this system of separately loading knob angles
  *       and data values, it is possible for the angles and values
  *       information to become 'out of sync' if the parameters are
  *       changed in the data block at the start of this file.
  *       A more flexible system would be to calculate the data values
  *       from the knob angles only as the angles are loaded or, even better,
  *       to calculate the knob angles from the values - as long as they
  *       are in the allowable range.
  */

    for(i=0;i<NCKNOBS;++i)
    {
      draw_pointer(gcd,&cknobs[i]);
      fread(&cknobs[i].angle,sizeof(float),1,fp);
      fread(&vp->cont[i],sizeof(float),1,fp);
      draw_pointer(gc,&cknobs[i]);
    }

    for(i=0;i<NDKNOBS;++i)
    {
      draw_pointer(gcd,&dknobs[i]);
      fread(&dknobs[i].angle,sizeof(float),1,fp);
      fread(&vp->dete[i],sizeof(unsigned char),1,fp);
      draw_pointer(gc,&dknobs[i]);
    }

    for(i=0;i<NBUTTONS-3;++i)
    {
      fread(&vp->on_off[i],sizeof(unsigned char),1,fp);
      draw_button(gc,gcd,&buttons[i],vp->on_off[i]);
    }

    fclose(fp);

    printf("\nPatch successfully loaded.\n");
  }
}

void save_patch(value *vp,control *cknobs,control *dknobs)
{
  FILE *fp;
  char name[108];
  const char *suffix=".Xpatch";
  int i;

  printf("\nCurrent patches are:\n");

  system("ls *.Xpatch");

  printf("\nName of patch to be saved (without the .Xpatch)?\n");

  scanf("%100s",name);

  strcat(name,suffix);

  printf("\nSaving patch information to file %s.\n",name);

  fp=fopen(name,"wb");

  for(i=0;i<NCKNOBS;++i)
  {
    fwrite(&cknobs[i].angle,sizeof(float),1,fp);
    fwrite(&vp->cont[i],sizeof(float),1,fp);
  }

  for(i=0;i<NDKNOBS;++i)
  {
    fwrite(&dknobs[i].angle,sizeof(float),1,fp);
    fwrite(&vp->dete[i],sizeof(unsigned char),1,fp);
  }

  for(i=0;i<NBUTTONS-3;++i)
  {
    fwrite(&vp->on_off[i],sizeof(unsigned char),1,fp);
  }

  fclose(fp);
}

TooSmall(Window win,GC gc,XFontStruct *font_info)
{
  char *string1="Too Small";
  int y_offset,x_offset;

  y_offset=font_info->max_bounds.ascent+2;
  x_offset=2;

  XDrawString(display,win,gc,x_offset,y_offset,string1,strlen(string1));
}
