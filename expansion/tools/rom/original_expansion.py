"""Original mathematics only. This module reads no external audio or analysis data.
25 periodic musical constructions; five explicitly experimental SFX constructions.
"""
import numpy as np
FOLDERS=['CORE MOTION','MODULATION','SPECTRAL MOTION','DIGITAL TONES','METAL & KEYS','HARMONY','SFX']
LABELS=[['BODY SWITCH','LOW ROTOR','DOUBLE EDGE','SYNC LATCH'],
 ['VECTOR FM','SIDEWINDER','PHASE BRAID','GLASS AM','SPIRAL WARP'],
 ['PARITY BRIDGE','WINDOW WALK','HARMONIC TILES','PRIME WALK'],
 ['BIT GARDEN','QUANT CROSS','RAIL STEPS'],
 ['TINE GARDEN','BELL PATH','BRIGHT WIRES','SOFT ALLOY'],
 ['MAJOR TRIAD','MINOR TRIAD','OPEN SUS4','MAJOR PENTA','MINOR PENTA'],
 ['TILT CLUSTER','PACKET RELAY','NOISE SHIFT','NOISE CELL','NOISE COUNTER']]
ROOTS=[[1]*4,[1]*5,[1]*4,[1,1,3],[1]*4,[16,20,24,24,30],[16,1,4,4,4]]
IDENTITIES=[
 ['Full hollow-to-solid bass; low and middle populations exchange','Rotating low harmonic pair with a stable root','Two edges trade width, spacing and polarity','Hard-sync-like integer cycle latch with changing seam'],
 ['Crossed FM operators with rising then retreating index','Ring-product sidebands exchange around the root','Two phase-warp operators braid and unbraid','Sparse amplitude sidebands change carrier emphasis','Asymmetric exponential phase warp with returning bend'],
 ['Odd/even populations exchange with independently rotating phases','A spectral window travels up then splits and returns','Four harmonic tiles change order in held states','Prime partial population migrates between registers'],
 ['Gray-code bit-plane melody in held digital states','Quantized FM ladder exchanges quantization and modulation','Ternary rail melody over a three-cycle root'],
 ['Sparse tine populations with changing upper resonance','Bell partial constellation exchanges upper islands','Bright string-like discrete partial groups','Soft metallic chord changes partial populations'],
 ['Fixed just major triad; root/third/fifth remain audible while octave colour changes','Fixed just minor triad; root/minor third/fifth with warm overtone exchange','Fixed suspended fourth chord; no minor/major third or changing pitch collection','Open major-pentatonic C-G-D-E-A voicing; root/third/fifth anchor softer second/sixth','Open minor-pentatonic C-G-Eb-F-Bb voicing; root/third/fifth anchor softer fourth/seventh'],
 ['Non-octave beating cluster with shifting dissonance','Three irregular wave packets trade width and location','Fine shift-register fizz with evolving colour and gaps','Sparse cellular crackle; bursts reorganize in discrete states','Coarse counter/XOR debris with pitched fragments and state jumps']]

HARMONY_BINS=[[16,20,24],[20,24,30],[24,32,36],[24,36,54,60,80],[30,45,72,80,108]]
def harmony_table(voice,n=512):
 """Stable just-tuned pitch sets: integer bins give exact periodic closure.
 Position changes relative voice/octave colour, never introduces another chord.
 Pentatonic voicings use octave spacing and quieter colour notes, not close clusters.
 """
 v=int(voice);bins=np.asarray(HARMONY_BINS[v]);u=np.arange(n)/n;angle=2*np.pi*u;rows=[]
 foundation=np.asarray([1,.78,.80] if v<3 else [1,.68,.32 if v==3 else .78,.74 if v==3 else .26,.32])
 phase=np.arange(len(bins))*(.11+.04*v)
 for j in range(16):
  t=j/15;theta=2*np.pi*t;weights=foundation*(.88+.12*np.cos(theta+np.arange(len(bins))*.85));weights[0]=1
  a=np.zeros(n)
  for i,b in enumerate(bins):
   a+=weights[i]*np.sin(b*angle+phase[i])
   # Gentle, voice-specific octave exchange; pitches and phase remain stable.
   if 2*b<n//2:a+=weights[i]*(.07+.10*(.5+.5*np.sin(theta+.8*i+v)))*np.sin(2*b*angle+2*phase[i])
   if 3*b<n//2:a+=weights[i]*.025*np.sin(3*b*angle+3*phase[i])
  a-=a.mean();a*=.20/np.sqrt(np.mean(a*a));rows.append(a)
 result=np.asarray(rows);result[-1]=result[0]
 assert np.max(np.abs(result))<.90
 return result

def source_table(folder,voice,n=None):
 g=FOLDERS.index(folder);display_v=int(voice);v=display_v;n=n or (512 if g==5 or (g==6 and v>=2) else 256)
 if g==5:return harmony_table(v,n)
 # The retained SFX keep their original algorithms after deleting SILK CHIRP.
 if g==6 and v>=1:v+=1
 H=4096;u=np.arange(H)/H;x=2*np.pi*u;k=np.arange(1,128,dtype=float)
 def add(bins,amps,phase=0):
  return (np.asarray(amps)[:,None]*np.sin(np.asarray(bins)[:,None]*x+np.asarray(phase)[...,None])).sum(axis=0)
 def partial(w,phase=0):return add(k,w,phase)
 states=[0,0,1,1,2,3,3,4,4,2,5,5,3,1,0,0]
 # Independent maximal-length bit sequence and cellular evolution. No reference data.
 register=511;bits=[]
 for _ in range(511):
  bits.append(register&1);feedback=((register>>0)^(register>>4))&1;register=(register>>1)|(feedback<<8)
 lfsr=np.asarray(bits)*2.-1
 cells=np.zeros(512,dtype=np.int64);cells[113]=cells[239]=cells[401]=1;cellstates=[]
 for j in range(100):
  left=np.roll(cells,1);right=np.roll(cells,-1);cells=(110>>((left<<2)|(cells<<1)|right))&1
  if j>=84:cellstates.append(cells.copy())
 rows=[]
 for j in range(16):
  t=j/15;s=np.sin(2*np.pi*t);r=np.sin(np.pi*t)**2;q=(1-np.cos(2*np.pi*t))/2;state=states[j]
  if g==0:
   if v==0:
    w=np.zeros(127);w[[0,1,2,3,5,7]]=np.array([1,.8*r,.65*(1-r),.5*q,.3*(1-q),.25*(1+s)/2]);a=partial(w)
   elif v==1:a=np.sin(x)+.9*np.cos(2*np.pi*t)*np.sin(2*x)+.65*np.sin(2*np.pi*t)*np.sin(3*x)+.32*r*np.sin(5*x+.8*s)
   elif v==2:
    d=.17+.31*r;shift=.37+.16*s;a=(u<d).astype(float)-.72*((u+shift)%1<(.12+.25*(1-r))).astype(float)
   else:
    cycles=[2,2,3,3,4,4,5,5,3,3,2,2,6,4,2,2][j];z=(u*cycles)%1;a=np.sin(2*np.pi*z+(.25+.5*r)*np.sin(x))*(.45+.55*(1-u))+.2*np.sin(x)
  elif g==1:
   if v==0:a=np.sin(x+(1.1+3.8*r)*np.sin(2*x+.7*s*np.sin(3*x)))+.33*np.sin(3*x+2*s*np.sin(x))
   elif v==1:a=(.4+.6*np.sin(2*x+.7*s))*(np.sin(3*x+1.3*r*np.sin(x)))+.22*np.sin(x)
   elif v==2:a=np.sin(x+(1.5+.9*s)*np.sin(x+.65*r*np.sin(3*x)))+.36*np.sin(2*x-1.1*s*np.sin(x))
   elif v==3:a=np.sin(x)*(.62+.38*np.cos(5*x+.9*s*np.sin(2*x)))+.5*r*np.sin(4*x)
   else:
    bend=.5+4*r;z=np.expm1(bend*u)/np.expm1(bend);a=np.sin(2*np.pi*z+.6*s*np.sin(2*x))
  elif g==2:
   if v==0:
    w=(.1+np.where(k.astype(int)%2,.9*(1-r),.9*r))/k**.55;w[0]+=.4;a=partial(w,.73*k*k+.7*s*np.sin(k*.43))
   elif v==1:
    center=5+23*r;w=(np.exp(-((k-center)/(3+5*q))**2)+.6*q*np.exp(-((k-8)/(3))**2))/k**.22;w[0]+=.4;a=partial(w,.83*k*k+.3*np.sin(k)*s)
   elif v==2:
    patterns=[[1,.08,.7,.08],[.08,.85,.08,.65],[.6,.08,.08,1],[.08,.6,1,.08],[1,.3,.08,.7],[.3,1,.7,.08]];w=np.asarray(patterns[state])[((k-1)//4).astype(int)%4]/k**.35;a=partial(w,.71*k*k+.4*np.sin(k*.8))
   else:
    bins=np.array([1,2,3,5,7,11,13,17,19,23,29,31]);amps=(.2+.8*np.cos(np.arange(12)*.5+2*np.pi*t)**2)/np.arange(1,13)**.3;a=add(bins,amps,.15*s*np.arange(12))
  elif g==3:
   ix=np.floor(u*64).astype(np.int64)
   if v==0:
    gray=ix^(ix>>1);weights=np.roll(np.array([.7,.42,.2,.11,.05,.025]),state);a=sum(weights[b]*(((gray>>b)&1)*2-1) for b in range(6))
   elif v==1:
    z=np.sin(x+(1+2*r)*np.sin(3*x+.5*s));steps=[2,3,4,6,8,5][state];a=np.round(z*steps)/steps
   else:
    rails=[3,5,7,4,9,6][state];z=np.sin(3*x+.5*s*np.sin(2*x));a=np.floor((z+1)*rails)/rails-1+.32*np.sin(6*x+.7*s)
  elif g==4:
   bins=[[1,2,4,7,11,16],[1,2,3,8,13,21],[1,2,5,9,14,22],[1,2,3,6,10,15]][v]
   amps=[1,.35+.2*r]+[(.1+.9*np.sin(np.pi*(t+i/7))**2)/(1+i*.2) for i in range(2,6)]
   a=add(bins,amps,.1*np.sin(np.asarray(bins)*.83)*r)+partial(.13/k**(1.35+.08*v))
  elif g==6 and v==0:
   bins=[16,17,19,23,31];b=np.array(bins);weights=.25+.75*np.cos(2*np.pi*t+np.arange(len(b))*.9)**2
   weights[0]+=.55;a=add(b,weights,.4*s*np.arange(len(b)))+add(2*b,.18*weights)+add(3*b,.06*weights)
  elif v==2:
   a=np.zeros(H)
   for h,center in enumerate([.15+.08*s,.47-.1*s,.79+.05*s]):
    distance=(u-center+.5)%1-.5;width=.036+.05*(.5+.5*np.sin(2*np.pi*t+h));a+=(1-.2*h)*np.exp(-(distance/width)**2)*np.cos((13+7*h)*2*np.pi*distance+.8*s*h)
  elif v==3:
   z=np.r_[lfsr,lfsr[0]];a=z[np.minimum((u*512).astype(int),511)];c=np.fft.rfft(a);freq=np.arange(len(c));c*=np.maximum(freq,1)**(-.7+.9*q);a=np.fft.irfft(c,H);a*=.55+.45*(np.sin(13*x+3*s)>-.7+.4*r)
  elif v==4:
   cell=cellstates[(state*3)%16];idx=(u*512).astype(int);edge=np.maximum(cell-np.roll(cell,1),0);a=edge[idx]*(1+.5*np.sin(23*x+.8*s));a-=.5*cell[idx];a*=.55+.45*(np.sin(5*x+2*s)>.15)
  else:
   ix=(u*128).astype(np.int64);word=((ix*(7+2*state))^(ix>>(1+state%3))^((ix<<2)&127))&127;a=word/63.5-1;a=np.round(a*(2+state))/(2+state);a*=.35+.65*(np.sin((3+state)*x)>.0)
  a-=a.mean();c=np.fft.rfft(a)/H;small=c[:n//2+1].copy();small[0]=small[-1]=0;rows.append(np.fft.irfft(small*n,n))
 result=np.asarray(rows);result[-1]=result[0]
 # Offline frame RMS leveling, baked into the ROM. No render-time auto-gain.
 rms=np.sqrt(np.mean(result*result,axis=1,keepdims=True));result*=.20/rms
 if np.max(np.abs(result))>.90:raise ValueError('RMS level exceeds source peak budget: '+LABELS[g][display_v])
 return result
