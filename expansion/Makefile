xsynth : main.o user_interface.o synth.o oscillator.o volume.o init_dsp.o \
         pitch_init.o midi.o
	gcc -o xsynth -static \
            main.o user_interface.o synth.o oscillator.o volume.o init_dsp.o \
            pitch_init.o midi.o /usr/X11R6/lib/libX11.a -lm

main.o : main.c my_types.h
	gcc -c main.c

user_interface.o : user_interface.c synth.h my_types.h xsynth_bitmap
	gcc -c -m486 -O3 -ffast-math -fforce-mem -fomit-frame-pointer \
            user_interface.c

synth.o : synth.c synth.h my_types.h
	gcc -c -m486 -O3 -ffast-math -fforce-mem -fomit-frame-pointer \
            synth.c

oscillator.o : oscillator.c synth.h
	gcc -c -m486 -O3 -ffast-math -fforce-mem -fomit-frame-pointer \
            oscillator.c

volume.o : volume.c
	gcc -c -m486 -O3 -ffast-math -fforce-mem -fomit-frame-pointer \
            volume.c

init_dsp.o : init_dsp.c
	gcc -c init_dsp.c

pitch_init.o : pitch_init.c
	gcc -c pitch_init.c

midi.o : midi.c my_types.h
	gcc -c -m486 -O3 -ffast-math -fforce-mem -fomit-frame-pointer \
            midi.c
