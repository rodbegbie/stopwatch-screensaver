"""Make every decimal literal in a C file a float, for single-precision builds.

`hacks/single_precision.h` turns the keyword `double` into `float`, but a
literal such as `0.5` is still a double, so `0.5 * x` with a float `x` is done
in double precision, which the ESP32-S3 does in software (Braid's draw loop
called `__muldf3` and `__adddf3` for every coordinate). Giving each literal an
`f` suffix keeps the arithmetic in single precision.

GCC's `-fsingle-precision-constant` does the same, but Apple clang ignores it,
so host tests would run different arithmetic from the board. A source rewrite
gives both the same text.

The copied hack stays byte-identical to upstream. The firmware build writes a
rewritten copy with `firmware/patch_float_literals.py` and compiles that.
"""

import re

_TOKEN = re.compile(
    r"""
      (?P<skip>
          /\*.*?\*/
        | //[^\n]*
        | "(?:\\.|[^"\\\n])*"
        | '(?:\\.|[^'\\\n])*'
      )
    | (?P<literal>
          (?:\d+\.\d+|\.\d+|\d+\.(?![\w.]))(?![\w.])
      )
    """,
    re.VERBOSE | re.DOTALL,
)


def float_literals(source: str) -> str:
    """Appends `f` to each decimal literal outside strings, characters and
    comments. A literal with a suffix or an exponent is left as it is."""
    count = 0

    def replace(match: re.Match[str]) -> str:
        nonlocal count
        if match.group("skip") is not None:
            return match.group(0)
        count += 1
        return match.group(0) + "f"

    out = _TOKEN.sub(replace, source)
    if count == 0:
        raise ValueError("no floating-point literals found; is this the right file?")
    return out
