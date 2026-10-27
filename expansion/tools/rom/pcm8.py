"""The exact signed PCM8 mapping auditioned in codec_prototype.py; no dither."""
from decimal import Decimal, localcontext, ROUND_HALF_EVEN
import math


def quantize(value):
    # Match the prototype's default Decimal context for v * 128 precisely.
    with localcontext() as ctx:
        ctx.prec=28
        v=value if isinstance(value,Decimal) else Decimal(str(value))
        if not v.is_finite():raise ValueError('Nonfinite PCM8 input')
        unbounded=int((v*128).to_integral_value(rounding=ROUND_HALF_EVEN))
        return max(-128,min(127,unbounded)),int(not -128<=unbounded<=127)


def decode_codes(data):
    return [(b if b<128 else b-256)*256 for b in data]
