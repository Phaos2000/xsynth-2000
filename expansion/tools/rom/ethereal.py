"""Original approved Halo/Prism recipes; stable PCM8 hashes recorded in tables.json.
No external waveform material. One table-global gain; no frame normalization.
"""
import numpy as np

def generate(kind):
 # Integer harmonic construction: periodic by design; stable phase anchor and
 # smoothly moving spectral/phase clusters. No recordings or reference tables.
 n=256;k=np.arange(1,121,dtype=float);phase=np.arange(n)/n
 frames=[]
 for j in range(16):
  t=j/15
  if kind=='A':
   w=.58/k**1.5 + .20*np.exp(-.5*((k-(7+9*t))/(2.8+1.8*t))**2)/k**.30
   w+=.09*np.exp(-.5*((k-(29-7*t))/5.5)**2)/k**.25
   w*=np.where(k%2,1,.50+.30*t)
   ph=.11*np.sin(k*1.71)+.045*np.sin(2*np.pi*t+k*.53)
  elif kind=='B':
   w=.65/k**1.65
   for c,a,width in [(4+4*t,.28,1.2),(14+6*t,.16,2.3),(31-7*t,.10,3.6)]:
    w+=a*np.exp(-.5*((k-c)/width)**2)/k**.25
   w*=1-.7*np.exp(-.5*((k-(10+5*t))/1.4)**2)
   ph=.17*np.sin(k*2.13)+.06*np.sin(np.pi*t)*np.sin(k*.73)
  else:
   w=.75/k**1.55
   w+=np.where(k%2,.28,.055)*np.exp(-.5*((k-(9+12*t))/5.8)**2)/k**.32
   w+=.11*np.exp(-.5*((k-(39-11*t))/7)**2)/k**.25
   w*=1-.65*np.exp(-.5*((k-(18-7*t))/2.2)**2)
   ph=.12*np.sin(k*1.31)+.055*np.sin(2*np.pi*t)*np.cos(k*.91)
  ph[0]=0
  # A small table-global intended level trajectory, never per-frame normalized.
  level=.88+.12*np.sin(np.pi*t)
  row=level*(w[:,None]*np.sin(2*np.pi*(k[:,None]*phase+ph[:,None]))).sum(axis=0)
  frames.append(row-row.mean())
 source=np.array(frames);gain=(28000/32768)/np.abs(source).max();source*=gain
 codes=np.rint(source*128).astype(int);assert codes.min()>=-128 and codes.max()<=127
 return codes.astype(np.int8),source,gain
