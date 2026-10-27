"""Shared deterministic recipe math, extracted unchanged from the historical prototype."""
from decimal import Decimal

PI = Decimal("3.1415926535897932384626433832795028841971693993751058209749445923078164062862089986")


def sine(turns: Decimal) -> Decimal:
    x = (turns % 1) * 2 * PI
    if x > PI:
        x -= 2 * PI
    term = total = x
    for k in range(1, 200):
        term *= -x * x / ((2 * k) * (2 * k + 1))
        total += term
        if abs(term) < Decimal("1e-85"):
            return total
    raise AssertionError("Taylor limit")


