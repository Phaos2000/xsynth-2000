"""Canonical PCM8-to-prepared-BAND derivation. No gain/DC correction.
The retained complex Fourier coefficients and >=256 addresses match production.
"""
import struct
import numpy as np

def prepared(tables,digest):
 chunks=[struct.pack('<II',0x44523850,len(tables)),digest]
 for t in tables:
  a=t['pcm'].astype('<f4');frames,n=a.shape
  c=np.fft.rfft(a.astype(float),axis=1)/n
  levels=[];ceiling=n
  while ceiling>=4:
   h=ceiling//2-1;bins=np.zeros((frames,max(256,n)//2+1),complex);bins[:,:h+1]=c[:,:h+1]
   levels.append((max(256,n),h,np.fft.irfft(bins*max(256,n),n=max(256,n),axis=1).astype('<f4')));ceiling//=2
  levels.append((1,0,c[:,0].real.astype('<f4').reshape(frames,1)))
  name=t['name'].encode('ascii');assert len(name)<44
  chunks.extend([struct.pack('<6I',t['id'],frames,n,t['rootBin'],t['familyId'],len(levels)),name+b'\0'*(44-len(name)),a.tobytes()])
  for n,h,x in levels:chunks.extend([struct.pack('<II',n,h),x.tobytes()])
 return b''.join(chunks)
