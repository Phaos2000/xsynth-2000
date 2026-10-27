"""All-frame wrap audit. A high slope is not itself a defective loop.
Flag wrap-local slope/curvature outliers; never force duplicate endpoints.
"""
from pathlib import Path
import sys,csv,json
import numpy as np
from generate import read_bank
def metrics(x):
    d=np.roll(x,-1)-x;dd=np.roll(d,-1)-d
    ordinary=d[:-1];interior_curvature=dd[1:-2]
    slope_limit=max(.0625,4*np.quantile(np.abs(ordinary),.95))
    curvature_limit=max(.09375,4*np.quantile(np.abs(interior_curvature),.95))
    flag=abs(d[-1])>slope_limit and max(abs(dd[-1]),abs(dd[-2]))>curvature_limit
    return dict(dc=float(x.mean()),rms=float(np.sqrt(np.mean(x*x))),peak=float(np.max(np.abs(x))),wrap_step=float(d[-1]),slope_before=float(d[-2]),slope_after=float(d[0]),wrap_curvature=float(max(abs(dd[-1]),abs(dd[-2]))),interior_slope_p95=float(np.quantile(np.abs(ordinary),.95)),interior_curvature_p95=float(np.quantile(np.abs(interior_curvature),.95)),suspicious=bool(flag))
def audit(bank,out):
    rows=[]
    for t in read_bank(bank.read_bytes()):
        for frame,x in enumerate(t['frames']):rows.append(dict(id=t['id'],frame=frame,rootBin=t['rootBin'],length=t['cycle_length'],**metrics(np.asarray(x)/32768.)))
    out.mkdir(parents=True,exist_ok=True)
    with (out/'cyclic-integrity.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
    summary=dict(frames=len(rows),suspicious=[r for r in rows if r['suspicious']],maj7=[r for r in rows if r['id']==32],not_duplicate_endpoint_test=True)
    (out/'cyclic-integrity.json').write_text(json.dumps(summary,indent=2));print(json.dumps(summary,indent=2))
    # Regression examples: a sampled sine crosses the wrap with valid slope;
    # an unperiodized ramp has an isolated wrap discontinuity.
    assert not metrics(np.sin(2*np.pi*np.arange(256)/256))['suspicious']
    assert metrics(np.linspace(-.8,.8,256))['suspicious']
    return rows
if __name__=='__main__':audit(Path(sys.argv[1]),Path(sys.argv[2]))
