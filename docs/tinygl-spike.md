# TinyGL spike (throwaway)

A probe to answer one question: can TinyGL, a software OpenGL, draw a
Pipes-sized scene on the StopWatch fast enough? It is not for merging, and
it is compiled only with `-DTGL_SPIKE`.

## Rebuilding it

The TinyGL copy is not committed. Rebuild it from upstream, pinned to the
C-Chads fork at commit `36a7987e7bebfda19615ea33341b1cc0ff9c3b13`:

```bash
git clone https://github.com/C-Chads/tinygl.git vendor/tinygl
git -C vendor/tinygl checkout 36a7987e7bebfda19615ea33341b1cc0ff9c3b13
mkdir -p firmware/spike_tinygl/src firmware/spike_tinygl/include
cp vendor/tinygl/src/*.c vendor/tinygl/src/*.h firmware/spike_tinygl/src/
cp -R vendor/tinygl/include/GL vendor/tinygl/include/zbuffer.h \
  vendor/tinygl/include/zfeatures.h firmware/spike_tinygl/include/
patch -p1 -d firmware/spike_tinygl < firmware/spike_tinygl.patch
```

The patch is ours: 16-bit pixels, a PSRAM-aware allocator, 64-slot
display-list buffers, a `memset_s` rename (macOS clash), `memory.c` including
its own header first, and single-precision literals and libm calls.

Then, from `firmware/`:

- `pio run -e spike_dump` builds for the host. Run
  `.pio/build/spike_dump/program <index> <frames> out.raw`, where the index is
  the last entry in `g_hacks[]`.
- `PLATFORMIO_BUILD_FLAGS='-DSTART_HACK=\"tglspike\" -DROTATE_SECONDS=0'
  pio run -e spike -t upload` flashes it and starts on the probe.
- `-DSPIKE_WIRE=1` switches to Pipes' wireframe primitives.
  `-DSPIKE_SIDES=<n>` and `-DSPIKE_SPHERE=<n>` change the tessellation.

## Licence

TinyGL is zlib-style, not MIT. Clause 1 requires an acknowledgment in the
product and its documentation, and altered versions must be marked as such.

## What it found

Pipes clears and redraws every piece so far on every frame, up to about 500
pieces per system. The probe does the same from display lists. Timings on the
S3 at 464 by 464, single precision, with Pipes' real strip primitives:

| Operation | Time |
| --- | --- |
| Full-frame clear (colour and z) | 24.8 ms |
| Full-frame copy to the canvas, byte swapped | 22.2 ms |
| Push to the display (existing) | 32 ms |
| 16 by 16 sphere (544 vertices) | 3.7 ms |
| 24-facet tube | 0.43 ms |

- A frame costs about 80 ms fixed plus 3.7 ms per piece: 2.8 fps at 80
  pieces, 1.2 fps at 210.
- Wireframe is no faster. Each vertex costs about 4 us before lighting, and
  lighting adds only about 2.8 us. The wire sphere has twice the vertices.
- TinyGL's display lists allocate a 16 KB buffer each, which overflowed
  PSRAM at 500 lists until the buffer was cut to 256 bytes.
- Stock TinyGL does software `double` maths in lighting and matrices. The
  single-precision patch took a piece from about 13 ms to 9 ms (with
  independent triangles).
- Vertex arrays (`glVertexPointer` plus `glDrawArrays`, as `unit_sphere` and
  `tube.c` use) do not work in TinyGL display lists. It counts stride in
  floats, not bytes, and reads the array when the list is replayed, by which
  time the hack has freed it. See the Gears section for the fix.

## Gears on the device

Gears (and its helpers `involute`, `tube`, `normals`, `rotator`,
`gltrackball`, `trackball` and `quaternion`) runs unmodified on the board.
Each is built by a small wrapper in `firmware/src/hacks/` that sets `USE_GL`
for that one file; `HAVE_GL` stays undefined so `ModeInfo` keeps its layout.
The GL layer is `firmware/src/glshim/`:

- `init_GL` opens TinyGL at 466 by 466 straight over the canvas, with its
  pixel packer patched (`TGL_SWAP_PIXELS`) to write the canvas's byte-swapped
  RGB565. There is no copy, and `glXSwapBuffers` only marks the canvas dirty.
- `gluPerspective`, `gluLookAt`, `glMateriali`, `glIsEnabled` and the
  framework's GL hooks are small stand-ins.
- The six vertex-array calls are routed to shim versions that read the arrays
  at the draw call with a byte stride and expand it into `glNormal3f` and
  `glVertex3f`, as real GL does.

TinyGL needed three fixes of its own (all in `spike_tinygl.patch`): display
list names start at 1 (name 0 means "invalid" to hacks), the buffer width is
no longer rounded down to a multiple of 4, and pixels are written swapped.

Gears picks a different arrangement every start, so its speed varies. With a
fixed seed (`-DGL_SPIKE_SEED=7`, a planetary set with a big ring) a frame is
17,706 vertices and 17,620 triangles:

| Part of the frame | Time |
| --- | --- |
| Clear (colour and z, PSRAM) | 24.6 ms |
| Fill (a quarter-size viewport saved 30 to 35 ms of it) | about 40 ms |
| Vertex and triangle setup | about 100 to 120 ms |
| Push to the display (existing) | 32 ms |

That is 4 to 5 fps for the heavy scene and about 9 fps for light ones.
Things that did not help, each tried alone on that scene: `-O2` for TinyGL
(about 1 percent), and the specular lookup tables in place of `pow` (none).
Building the whole firmware at `-O2` crashes the compiler in
`braid_single.c`. The remaining cost is per-vertex and per-triangle setup,
probably float divides, which the S3's FPU does in software.

For a merge this would need: byte-identical copies of the helpers with
notices (check `trackball.c`'s SGI licence first), tests for the GL layer
written first and mutation-checked, and a way to free the TinyGL context when
a GL hack stops (it is only replaced when the next GL hack starts).
