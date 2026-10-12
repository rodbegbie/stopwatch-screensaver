"""Makes the number of blocks in BlockTube's tunnel a build setting.

Upstream moves and draws 1,000 blocks a frame, 24 vertices each, and the board
spends about 100 ms a frame on those vertices alone. firmware/src/hacks/
blocktube_single_gl.c defines BLOCKTUBE_ENTITIES, and the build
(firmware/patch_float_literals.py) runs this on the hack's text. The copied
blocktube.c stays byte-identical; the replacement is checked, so a change
upstream fails the build.
"""

OLD = "#define MAX_ENTITIES     1000\n"
NEW = "#define MAX_ENTITIES BLOCKTUBE_ENTITIES\n"


def count_the_blocks(source: str) -> str:
    found = source.count(OLD)
    if found != 1:
        raise ValueError(
            f"expected 1 of `{OLD.strip()}` in blocktube.c, found {found}; has upstream changed?"
        )
    return source.replace(OLD, NEW)
