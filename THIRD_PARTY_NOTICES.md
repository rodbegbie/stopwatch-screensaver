# Third-party notices

This project is MIT licensed (see `LICENSE`). Files copied from other
projects keep their own copyright and licence notices, and are not
relicensed.

## xscreensaver

Hack sources copied from xscreensaver 6.16
(<https://www.jwz.org/xscreensaver/>) are under the following permission
notice. The copyright line of each copied file is listed in the table
below.

```text
Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.  No representations are made about the suitability of this
software for any purpose.  It is provided "as is" without express or
implied warranty.
```

| File | Copyright | Licence |
| --- | --- | --- |
| `firmware/src/hacks/pyro/pyro.c` | Copyright (c) 1992-2008 Jamie Zawinski; inspired by TI Explorer Lisp code by John S. Pezaris | jwz permission notice (above) |
| `firmware/src/xs_support/hsv.c` | Copyright (c) 1992, 1997 Jamie Zawinski | jwz permission notice (above) |
| `firmware/src/xs_support/xlockmore.c` | Copyright (c) 1997-2018 Jamie Zawinski | jwz permission notice (above) |
| `firmware/src/xs_support/xlockmore.h` | Copyright (c) 1997-2021 Jamie Zawinski | jwz permission notice (above) |
| `firmware/src/xs_support/xlockmoreI.h` | Copyright (c) 1997-2025 Jamie Zawinski | jwz permission notice (above) |
| `firmware/src/hacks/hypercube/hypercube.c` | Copyright (c) 1992-2008 Jamie Zawinski | jwz permission notice (above) |
| `firmware/src/hacks/xspirograph/xspirograph.c` | The Spiral Generator, Copyright (c) 2000 Rohit Singh; contains code from xscreensaver, Copyright (c) 1992, 1995, 1996, 1997 Jamie Zawinski | jwz permission notice (above); the file's own header says "notices" for its two copyright holders |
| `firmware/src/hacks/petri/petri.c` | Copyright (c) 1992-1999 Dan Bornstein, with help from Jamie Zawinski | jwz permission notice (above) |
| `firmware/src/hacks/helix/helix.c` | Copyright (c) 1992-2008 Jamie Zawinski | jwz permission notice (above) |
| `firmware/src/hacks/rorschach/rorschach.c` | Copyright (c) 1992-2014 Jamie Zawinski | jwz permission notice (above) |
| `firmware/src/hacks/coral/coral.c` | Frederick G.M. Roeber, 1997 (no copyright line; the header names him as author) | jwz permission notice (above) |
| `firmware/src/hacks/squiral/squiral.c` | Jeff Epler, 1999 (no copyright line; the header names him as author) | jwz permission notice (above) |
| `firmware/src/hacks/critical/critical.c` | Copyright (C) 1998, 1999, 2000 Martin Pool | jwz permission notice (above), reflowed in the file |
| `firmware/src/hacks/cloudlife/cloudlife.c` | Don Marti; based on xscreensaver, Copyright (c) 1997, 1998, 2002 Jamie Zawinski | jwz permission notice (above) |
| `firmware/src/hacks/whirlwindwarp/whirlwindwarp.c` | Copyright (c) 2000 Paul "Joey" Clark | jwz permission notice (above) |
| `firmware/src/hacks/flame/flame.c` | Copyright (c) 1993-2014 Jamie Zawinski; ported from xlock, Copyright (c) 1991 Patrick J. Naughton, with updates by Scott Draves | jwz permission notice (above), and Naughton's xlock notice (below) |
| `firmware/src/hacks/pedal/pedal.c` | Copyright (c) 1994 Carnegie Mellon University; X version by Dale Moore | CMU permission notice (below) |
| `firmware/src/hacks/hopalong/hopalong.c` | Copyright (c) 1991 Patrick J. Naughton; later changes by the xlockmore and xscreensaver authors | xlock permission notice (below) |
| `firmware/src/hacks/vines/vines.c` | Copyright (c) 1997 Tracy Camp | xlock permission notice (below) |
| `firmware/src/hacks/sierpinski/sierpinski.c` | Copyright (c) 1996 Desmond Daignault | xlock permission notice (below) |
| `firmware/src/hacks/fadeplot/fadeplot.c` | Copyright (c) 1996 Charles Vidal | xlock permission notice (below) |
| `firmware/src/hacks/thornbird/thornbird.c` | Copyright (c) 1996 Tim Auckland | xlock permission notice (below) |
| `firmware/src/hacks/spiral/spiral.c` | Copyright (c) 1994 Darrick Brown | xlock permission notice (below) |
| `firmware/src/hacks/sphere/sphere.c` | Copyright (c) 1988 Sun Microsystems | xlock permission notice (below) |
| `firmware/src/hacks/discrete/discrete.c` | Copyright (c) 1996 Tim Auckland | xlock permission notice (below) |
| `firmware/src/hacks/galaxy/galaxy.c` | No copyright line; the header credits Uli Siegmund, Harald Backert and Hubert Feyrer (1997) | xlock permission notice (below) |
| `firmware/src/hacks/drift/drift.c` | Copyright (c) 1991 Patrick J. Naughton | xlock permission notice (below) |
| `firmware/src/hacks/lightning/lightning.c` | Copyright (c) 1996 Keith Romberg | xlock permission notice (below) |
| `firmware/src/hacks/maze/maze.c` | Copyright 1988 by Sun Microsystems, Inc.; later changes by Dave Lemke, Richard Hess, Jim Randell, Ed James, Johannes Keukelaar, Zack Weinberg and Jamie Zawinski | Sun/MIT permission notice (below) |

Each copied file keeps its own header unchanged; the notice text above is
reproduced here to satisfy the "supporting documentation" requirement.

### xlock permission notice (Flame and the xlockmore hacks)

`flame.c` was ported from xlock and carries Patrick J. Naughton's notice
alongside jwz's. The xlockmore hacks (`hopalong.c`, `vines.c` and the
others whose table rows say "xlock permission notice") carry the same
permission text alone, each under its own author's copyright line, which
the table lists. The text is reproduced here as it appears in
`hopalong.c`:

```text
Copyright (c) 1991 by Patrick J. Naughton.

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation.

This file is provided AS IS with no warranties of any kind.  The author
shall have no liability with respect to the infringement of copyrights,
trade secrets or any patents by this file or any part thereof.  In no
event will the author be liable for any lost revenue or profits or
other special, indirect and consequential damages.
```

### Carnegie Mellon University notice (Pedal)

`pedal.c` carries its own permission notice, reproduced here as it appears
in the file (including its "fnord" typos):

```text
Copyright (c) 1994, by Carnegie Mellon University.  Permission to use,
copy, modify, distribute, and sell this software and its documentation
for any purpose is hereby granted without fee, provided fnord that the
above copyright notice appear in all copies and that both that copyright
notice and this permission notice appear in supporting documentation.
No representations are made about the  suitability of fnord this software
for any purpose.  It is provided "as is" without express or implied
warranty.
```

### Sun Microsystems and MIT notice (Maze)

`maze.c` carries its own permission notice, reproduced here as it appears
in the file:

```text
Copyright 1988 by Sun Microsystems, Inc. Mountain View, CA.

All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the names of Sun or MIT not be
used in advertising or publicity pertaining to distribution of the
software without specific prior written permission. Sun and M.I.T.
make no representations about the suitability of this software for
any purpose. It is provided "as is" without any express or implied warranty.

SUN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE. IN NO EVENT SHALL SUN BE LIABLE FOR ANY SPECIAL, INDIRECT
OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
OR PERFORMANCE OF THIS SOFTWARE.
```
