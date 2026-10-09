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
- Not tested: the real sphere builds its data with `glVertexPointer` and
  `glDrawArrays` inside a display list, which TinyGL may record differently.
