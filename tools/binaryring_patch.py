"""Makes BinaryRing's random scale a float literal.

`frand1()` is `random() << 1` times 4.65661287524579692411E-10, an exponent
literal, which tools/float_literals.py leaves as a double. Every particle calls
it twice a frame, so in the single-precision build each call would still be a
software double multiply on the board. The copied binaryring.c stays
byte-identical; firmware/patch_float_literals.py runs this on its text.
"""

OLD = "4.65661287524579692411E-10)"
NEW = "4.65661287524579692411E-10f)"


def float_random_scale(source: str) -> str:
    found = source.count(OLD)
    if found != 1:
        raise ValueError(
            f"expected 1 of `{OLD}` in binaryring.c, found {found}; has upstream changed?"
        )
    return source.replace(OLD, NEW)
