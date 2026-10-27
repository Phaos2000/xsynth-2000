"""Audited fixed recipe operations. JSON supplies data, never executable expressions."""
from decimal import Decimal as D, localcontext, ROUND_HALF_EVEN
from functools import lru_cache
from codec_prototype import sine


def lerp(a, b, t):
    return D(a)+(D(b)-D(a))*t


def gauss(k, center, width):
    return (-((D(k)-D(center))/D(width))**2/2).exp()


def fold(x):
    return 1-abs(((x+1) % 4+4) % 4-2)


def phase(k, seed):
    return D((k*k*seed) % 251)/251


@lru_cache(maxsize=128)
def basis(n, maximum, seed):
    return tuple(tuple(sine(D(k)*j/n+phase(k, seed)) for j in range(n))
                 for k in range(1, maximum+1))


def additive(n, weights, seed=0):
    waves=basis(n,len(weights),seed)
    return [sum((a*w[j] for a,w in zip(weights,waves)),D(0)) for j in range(n)]


def curved_grid(x, count):
    p=x*count; i=int(p); f=p-i
    return i,f


def quantized_curve(p, grid, levels, kind, noise_seed=0):
    # Interpolate discrete source-quantizer settings, not oscillator playback.
    def value(g,l):
        q=(p*g).to_integral_value(rounding='ROUND_FLOOR')/g
        if kind=='stairs':
            raw=D('.65')*(1-4*abs(q-D('.5')))+D('.35')*sine(q)
        else:
            raw=sum((D(a)*sine(k*q+phase(k,19)) for k,a in
                     ((1,'0.3'),(4,'0.35'),(9,'0.23'),(13,'0.19'),(21,'0.11'),(27,'0.07'))),D(0))
            raw+=sum((D('.012')*D(3+(k*29+noise_seed)%19)/22*sine(k*q+phase(k,noise_seed))
                      for k in range(16,31) if k%5),D(0))
        return (raw*l).to_integral_value(rounding=ROUND_HALF_EVEN)/l
    g0=int(grid);l0=int(levels);gf=grid-g0;lf=levels-l0
    return ((1-gf)*((1-lf)*value(g0,l0)+lf*value(g0,l0+1))+
            gf*((1-lf)*value(g0+1,l0)+lf*value(g0+1,l0+1)))


def render_frame(r, t):
    n=r['cycle_length'];op=r['operation'];p=r['parameters'];seed=r['phase_seed']
    maximum=min(n//2-1,p.get('harmonics',n//2-1))
    weights=[]
    if op=='pulse':
        duty=lerp(*p['duty'],t)
        return [D(1) if D(i)/n<duty else D(-1) for i in range(n)]
    if op in ('stairs','grain'):
        grid=lerp(*p['phase_grid'],t);levels=lerp(*p['amplitude_levels'],t)
        return [quantized_curve(D(i)/n,grid,levels,op,p['noise_seed']) for i in range(n)]
    if op=='logic':
        position=t*(len(p['masks'])-1);a=min(int(position),len(p['masks'])-2);f=position-a
        # Explicit phase-counter bitplane XOR/AND masks. Blend a fixed sequence.
        def word(i,m):
            return D(((i ^ ((i>>1)&m[0]) ^ ((i<<1)&m[1]))&m[2]))/31-D('.5')
        return [(1-f)*word(i,p['masks'][a])+f*word(i,p['masks'][a+1]) for i in range(n)]
    if op=='fold':
        return [fold(lerp(*p['drive'],t)*sine(D(i)/n)+lerp(*p['asymmetry'],t)) for i in range(n)]
    if op in ('metal','pad_metal'):
        values=[]
        stretch=lerp(*p['stretch'],t)
        partials=[]
        for k in range(1,p['partials']+1):
            ratio=D(k)**stretch
            if op=='metal':
                partials.append((ratio,(1+t*D(k)/p['partials'])/D(k)**D('1.3'),phase(k,seed)))
            else:
                partials.append((D('2.3')*ratio+D('.7'),(1-t*D('.45'))/D(k),phase(k,seed)))
                partials.append((D('19.7')+D('1.81')*ratio,(D('.12')+t*D('.9'))/D(k)**D('.7'),phase(k,seed+2)))
        for j in range(n):
            u=D(j)/n; v=D(0)
            for ratio,amplitude,start_phase in partials:
                v+=amplitude*sine(ratio*u+start_phase)
            # Smooth deterministic finite-segment periodicization, not playback filtering.
            values.append(v*sine(u/2)**2)
        return values
    if op=='drawbar':
        pos=t*(len(p['gains'])-1);a=min(int(pos),len(p['gains'])-2);f=pos-a
        gains=[lerp(x,y,f) for x,y in zip(p['gains'][a],p['gains'][a+1])]
        weights=[D(0)]*max(p['bins'])
        for k,g in zip(p['bins'],gains):weights[k-1]=g
        return additive(n,weights,seed)
    if op=='chord':
        weights=[D(0)]*maximum
        for component,b in enumerate(p['bins']):
            body=lerp(*p['component_weights'][component],t)
            for overtone in range(1,p['overtones']+1):
                k=b*overtone
                if k>maximum:break
                amp=body*(D(1) if overtone==1 else lerp('0.04','0.7',t)/D(overtone)**D('1.15'))
                # Different component envelopes; no extra running oscillators.
                if p['color']=='minor':amp*=gauss(k,D(b)+5+10*t,12)+D('.25')
                if p['color']=='ninth' and component==3:amp*=D('1.3')
                weights[k-1]+=amp
        return additive(n,weights,seed)
    for k in range(1,maximum+1):
        x=D(k)
        if op=='spectral':
            a=(-x/lerp(*p['cutoff'],t)).exp()/x
        elif op=='gaps':
            # Fixed occupancy anchors, raised-cosine crossfades; no reseeding.
            pos=t*3;stage=min(int(pos),2);f=(1-sine((pos-stage)/2+D('.25')))/2
            masks=p['populations']
            g0=D(1) if (k%masks[stage][0]) in masks[stage][1] else D('.015')
            g1=D(1) if (k%masks[stage+1][0]) in masks[stage+1][1] else D('.015')
            a=lerp(g0,g1,f)/x**D('.75')
        elif op=='vowel':
            centers=p['centers'];u=t*2;stage=min(int(u),1);f=u-stage
            a=sum((gauss(k,lerp(c0,c1,f),w) for c0,c1,w in zip(centers[stage],centers[stage+1],p['widths'])),D(0))/x**D('.35')
        elif op=='comb':
            d=lerp(*p['spacing'],t);a=(D('.15')+D('.85')*sine(x*d/2+D('.25'))**2)/x
        elif op=='horn':
            a=(D('.25')+gauss(k,lerp('3','13',t),lerp('2','7',t)))/x**D('.8')
            if k%2==0:a*=lerp('.12','1.4',t)
        elif op=='string':
            a=x**D('-1.2')*lerp(((-x/5).exp()),D(1)+x/30,t)
            a*=D('.20')+D('2')*gauss(k,D(p['body_centers'][0]),2)+D('1.8')*gauss(k,D(p['body_centers'][1]),4)+2*t*gauss(k,D(p['body_centers'][2]),5)
            if k%9 in (3,7):a*=D('.35')
        elif op=='pluck':
            a=sine(x*lerp(*p['pluck_point'],t)/2)/x**D('1.3')*(-x/lerp(*p['cutoff'],t)).exp()
        elif op=='flute':
            a=D(1) if k==1 else lerp(*p['second_gain'],t) if k==2 else lerp(*p['third_gain'],t) if k==3 else (D(p['upper_noise_gain'])*t*D((k*37+p['noise_seed'])%29)/29 if k>=6 else D(0))
        elif op=='reed':
            a=(D('.4')+gauss(k,5,2)+lerp('.1','1.1',t)*gauss(k,11,3))/x
            if k%2==0:a*=lerp('.02','.65',t)/x.sqrt()
        elif op=='voice':
            a=(gauss(k,lerp('3','7',t),D('1.6'))+D('.8')*gauss(k,lerp('10','22',t),3))/x.sqrt()
            if k>25:a+=D('.018')*t*D((k*31+p['noise_seed'])%37)/37
        elif op=='bass':
            a=D(1) if k==1 else (D('.14')+t*D('.65'))/(x*x)
            if k%2==0:a*=lerp('.05','.85',t)
        elif op=='keys':
            a=lerp(p['weights_start'].get(str(k),'0'),p['weights_end'].get(str(k),'0'),t)
        elif op=='air':
            a=(D('.16')/x if k in (1,3,5) else D(0))
            if k>=12 and k%11 not in (0,1,2):
                a+=(gauss(k,lerp('21','37',t),lerp('5','13',t))+D('.6')*gauss(k,73,lerp('3','18',t)))*D('.11')*D(8+(k*17+p['noise_seed'])%23)/31
        elif op=='dream':
            a=D('.55')/x**2 if k<=5 else D(0)
            a+=sum((D(amp)*gauss(k,c,lerp('1.2','4.8',t)) for c,amp in ((13,'.15'),(31,'.10'),(57,'.07'))),D(0))*(D('.1')+t)
        elif op=='ethereal':
            a=D('.015') if k==1 else D(0)
            if k%2 and k>25:
                a+=(gauss(k,lerp('41','65',t),8)+D('.55')*gauss(k,97,12))*D('.18')
        elif op=='ambient':
            # Complementary broad bands; deliberately stable low/mid coverage.
            a=(D('.18')+gauss(k,8,6)+D('.75')*gauss(k,26,8))/x**D('.35')
            a*=1-(D('.2')+D('.65')*t)*gauss(k,13,4)
            a*=1-(D('.85')-D('.55')*t)*gauss(k,29,5)
        elif op=='epic':
            a=sum((D(amp)*gauss(k,c,w) for c,w,amp in ((3,1,'.65'),(16,3,'.45'),(64,9,'.3'))),D(0))
            a*=D('.15')+t*x/35 if k>10 else D(1)-t*D('.25')
            if k%2:a*=D('.65')
        elif op=='dark':
            a=(gauss(k,3,2)+lerp('.1','.8',t)*gauss(k,9,3))/x**D('.5')
            if k%2==0:a*=D('.035')
            a*=1-D('.9')*gauss(k,20,5);a*=1-D('.8')*gauss(k,38,8)
            if k>35:a+=D('.003')*t*D((k*13+p['noise_seed'])%17)/17
        elif op=='glass':
            a=D('.006') if k==1 else D(0)
            a+=sum((D(amp)*gauss(k,c,lerp('.8','3.6',t)) for c,amp in ((23,'.40'),(52,'.25'),(89,'.16'))),D(0))
        elif op=='pad_choir':
            a=(gauss(k,lerp('7','11',t),4)+D('.8')*gauss(k,27,lerp('3','9',t))+D('.65')*gauss(k,49,12))/x**D('.2')
            if k>15:a+=D('.04')*D((k*41+p['noise_seed'])%43)/43
        elif op=='motion':
            group=k%3
            bands=(lerp('.8','.18',t),lerp('.15','.9',t),D('.35')+D('.55')*t*t)
            a=bands[group]*(D('.3')+gauss(k,lerp('9','32',t),11))/x**D('.45')
            a*=1-D('.85')*gauss(k,lerp('18','43',t),3)
        else:raise ValueError('Unknown recipe operation: '+op)
        weights.append(a)
    values=additive(n,weights,seed)
    if op in ('bass','keys'):
        amount=t*(D('.9') if op=='bass' else D('.18'))
        values=[(1-amount)*v+amount*fold(v*(1+t*3)+t*D('.22')) for v in values]
    return values


def table_sources(recipe):
    with localcontext() as ctx:
        ctx.prec=80
        frames=[]
        for j in range(16):
            row=render_frame(recipe,D(j)/15)
            dc=sum(row)/len(row)
            frames.append([v-dc for v in row])
        peak=max(abs(v) for row in frames for v in row)
        if not peak:raise ValueError('Silent recipe')
        gain=D(recipe['peak_target_s16'])/32768/peak
        # Q31 source diagnostic: high resolution, never used for playback directly.
        return [[int((v*gain*(1<<31)).to_integral_value(rounding=ROUND_HALF_EVEN)) for v in row] for row in frames]
