#define NCKNOBS 24
#define NDKNOBS 3
#define NBUTTONS 5

typedef struct                      /* information required for a controller */
{
  unsigned char type;          /* 1=continuous knob 2=detented knob 4=button */
  Window win;                                           /* window identifier */
  unsigned int border_width;                                 /* border width */
  int x,y;                              /* position in main window of origin */
  unsigned int width,height;                      /* window width and height */
  float radius;                                 /* knob radius within window */
  float angle;                                      /* current pointer angle */
  unsigned char lin_log;                /* linear 0 or logarithmic 1 control */
  float a;                                   /* continuous control parameter */
  float b;                                   /* continuous control parameter */
  float c;                                   /* continuous control parameter */
  char *label;                                               /* control name */
} control;

typedef struct
{
  float cont[NCKNOBS];
  unsigned char dete[NDKNOBS];
  unsigned char on_off[NBUTTONS];                           /* on=1 or off=0 */
} value;

typedef struct
{
  unsigned char note_table[128];
  float pitch_wheel;
  float mod_wheel;
} midi_info;
