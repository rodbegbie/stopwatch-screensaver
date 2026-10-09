"""Patches that keep Pacman inside the board's 16 KB loop-task stack.

1. patch_pacman_level: always use the fixed level (below).
2. patch_pacman_ai: turn the ghosts' recursive route search into a loop (end of
   this file).

Make pacman_level.c always use its fixed level.

`pacman_createnewlevel` builds half its levels with a random generator
(`creatlevelblock` and `nextstep`) and copies the fixed `stdlevel` for the
rest. The generator calls itself once per tile and keeps a 1.3 KB copy of the
level in every frame, so a level takes up to about 300 KB of stack. The board's
loop task has 16 KB, and Pacman reboots the board on its first frame when the
generator runs.

The copied pacman_level.c stays byte-identical to upstream. The firmware build
writes a patched copy with `firmware/patch_pacman.py` and compiles that
instead. The generator is still compiled but never called.
"""

import hashlib

RANDOM_LEVEL_TEST = "if (NRAND (2) == 0) {"
FIXED_LEVEL_COPY = "memcpy (level, stdlevel, sizeof (lev_t))"
REPLACEMENT = "if (0) { /* patched: the random generator needs ~300 KB of stack */"


def patch_pacman_level(source: str) -> str:
    found = source.count(RANDOM_LEVEL_TEST)
    if found != 1:
        raise ValueError(
            f"expected one `{RANDOM_LEVEL_TEST}` (the NRAND choice in "
            f"pacman_createnewlevel), found {found}; has upstream changed?"
        )
    if FIXED_LEVEL_COPY not in source:
        raise ValueError(
            f"`{FIXED_LEVEL_COPY}` (the stdlevel branch) is gone; has upstream changed?"
        )
    return source.replace(RANDOM_LEVEL_TEST, REPLACEMENT)


BACK_TRACK_START = "static int\nrecur_back_track ("
BACK_TRACK_END = "static void\nfind_home"
BACK_TRACK_SHA256 = "5a15acb09a9f8b768b65d3630f9a1eb35956c0627830a6f49d0f0de78939dfbc"

# A depth-first search that visits the cells in the same order as upstream's
# recursion (left, up, down, right), saves positions and stores directions in
# the same order, but keeps its frames in a heap array. The recursion reached
# 453 levels of 48 bytes from some cells (about 22 KB, against a 16 KB loop
# stack); a path cannot be deeper than the ghost's trace, GHOST_TRACE cells.
NEW_BACK_TRACK = """typedef struct {
    int row, col, next;
} bt_frame;

static const pos bt_order[4] = { pos_left, pos_up, pos_down, pos_right };

/* Enters the cell (row, col) as recur_back_track did on a call: returns False
 * or True if the call would return at once, and -1 after pushing a frame. */
static int
bt_enter ( ghoststruct *g, bt_frame *stack, int *top, int row, int col ){
    if ( already_tried ( g, col, row ) )
        return False;

    if ( found_jail ( col, row ) )
        return True;

    save_position ( g, col, row );
    (*top)++;
    stack[*top].row = row;
    stack[*top].col = col;
    stack[*top].next = 0;
    return -1;
}

/* Patched: the recursion is a loop over a heap stack, in the same order. */
static int
recur_back_track ( pacmangamestruct * pp, ghoststruct *g, int row, int col ){
    bt_frame *stack = (bt_frame *) malloc ( sizeof ( bt_frame ) * GHOST_TRACE );
    int top = -1, ret;

    if ( stack == NULL )
        return False;

    ret = bt_enter ( g, stack, &top, row, col );
    while ( top >= 0 ){
        bt_frame *f = &stack[top];
        int descended = False;

        if ( ret == True ){
            /* the call just made returned True: this frame stores its move */
            store_dir ( g, bt_order[f->next - 1] );
            top--;
            continue;
        }

        while ( f->next < 4 ){
            int new_row, new_col;
            pos d = bt_order[f->next++];

            if ( move_ghost ( pp, f->row, f->col, d, &new_row, &new_col )){
                ret = bt_enter ( g, stack, &top, new_row, new_col );
                descended = True;
                break;
            }
        }
        if ( !descended ){
            top--;
            ret = False;
        }
    }
    free ( stack );
    return ret;
}

"""


def patch_pacman_ai(source: str) -> str:
    found = source.count(BACK_TRACK_START)
    if found != 1:
        raise ValueError(
            f"expected one `recur_back_track` definition, found {found}; "
            "has upstream pacman_ai.c changed?"
        )
    start = source.index(BACK_TRACK_START)
    end = source.find(BACK_TRACK_END, start)
    if end < 0:
        raise ValueError("find_home no longer follows recur_back_track; has upstream changed?")
    original = source[start:end]
    if hashlib.sha256(original.encode()).hexdigest() != BACK_TRACK_SHA256:
        raise ValueError(
            "recur_back_track has changed upstream; check the rewrite still "
            "visits cells and stores directions in the same order"
        )
    return source[:start] + NEW_BACK_TRACK + source[end:]
