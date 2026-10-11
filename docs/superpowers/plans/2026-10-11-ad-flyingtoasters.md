# ad-flyingtoasters Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use
> checkbox (`- [ ]`) syntax for tracking.

**Goal:** A new hack, `ad-flyingtoasters`, that recreates After Dark 2.0's
Flying Toasters with the original sprites, extracted at build time from the
user's own disk image.

**Architecture:** A uv tool reads the HFS floppy image, decodes ten PICTs
and writes a C header into the build directory. A PlatformIO pre-script runs
it only when the image is present and then defines
`HAVE_AFTER_DARK_TOASTERS`. The hack is three C units: a pure simulation, a
sprite builder, and the `HackEntry` glue. Only the glue needs the header.

**Tech Stack:** Python 3.13 with uv, pytest and `machfs` (MIT); C on
PlatformIO (ESP32-S3 Arduino, native host with Unity and ASan); the repo's
X11 shim (`XCreateImage`, `XPutImage`).

**Spec:** `docs/superpowers/specs/2026-10-11-flying-toasters-design.md`.
Read it first: the "Behaviour" section is the source of every number below.

## Global Constraints

- No Berkeley Systems bytes in git: no sprites, PICT data, disassembly or
  fixtures cut from the disk. Test PICTs and resource forks are built by the
  tests themselves.
- Disk image: `vendor/after-dark-v2.0v-800k` (git-ignored). The environment
  variable `AFTER_DARK_IMAGE` overrides the path (Claude's call, added so the
  absent and broken cases can be built without touching the shared
  `vendor/` symlink).
- A build without the image succeeds and leaves the hack out. An image that
  is present but fails to extract fails the build.
- Hack `name` is `"ad-flyingtoasters"`, symbol `ad_flyingtoasters_hack`,
  directory `firmware/src/hacks/ad_flyingtoasters/`, last in `g_hacks[]`.
- Our code is MIT. Copied-hack rules (byte-identical, notices row) do not
  apply: nothing is copied.
- Python: 3.13+, uv, ruff (`ruff check` and `ruff format --check` on the
  files you touch only), pytest. Markdown: invoke the `/markdown` skill, run
  `markdownlint` from the repo root.
- Commits end with the session's `Co-Authored-By` trailer. `git add` named
  paths only. This is a worktree: follow `AGENTS.md`'s worktree gotchas (run
  `uv` and `pio` one at a time, absolute paths, no `&&` chains with computed
  paths).
- Never flash the device without asking Rod.

## Review Focus

1. A flyer partly or wholly off the canvas (spawns sit just outside it, and
   the corners of the round panel): blitting must clip without an ASan error
   or a wrap into the next row. Pinned by `test_blit_clips_at_every_edge` in
   Task 6.
1. A disk image that is not the expected one (DiskCopy 4.2 with an 84-byte
   header, another After Dark version, a truncated file): the tool must fail
   naming what it could not find, not write a half header. Pinned by
   `test_rejects_image_of_unexpected_size` and
   `test_missing_pict_is_named_in_the_error` in Task 2.
1. A crowded screen where every placement try collides: the slot stays empty
   and is retried, with no infinite loop. Pinned by
   `test_slot_stays_empty_after_four_colliding_tries` in Task 4.
1. The hack started and stopped many times by rotation: no leak of the five
   sprite images. Pinned by `test_restart_does_not_leak` in Task 6 (runs
   only with the image).
1. A force flag with a bad value (`-DAD_TOASTERS_ART=2`): it must stop the
   build, not quietly pick at random. Pinned by an `#error` in Task 6, with a
   compile check in its verification step.

---

## File Structure

| Path | Responsibility |
| --- | --- |
| `tools/after_dark_sprites.py` | uv script: image → sprites header |
| `tools/tests/test_after_dark_sprites.py` | its tests (synthetic data) |
| `firmware/after_dark_sprites.py` | PlatformIO pre-script |
| `firmware/platformio.ini` | add the pre-script to all three envs |
| `firmware/src/hacks/ad_flyingtoasters/ad_sim.[ch]` | slots, spawning, motion, collision, flap |
| `firmware/src/hacks/ad_flyingtoasters/ad_sprites.[ch]` | doneness, mono expansion, 70×70 sprite build, picks |
| `firmware/src/hacks/ad_flyingtoasters/ad_flyingtoasters.c` | `HackEntry` glue, whole file under `#ifdef HAVE_AFTER_DARK_TOASTERS` |
| `firmware/src/hacks/registry.c` | register it last, under the `#ifdef` |
| `firmware/test/test_ad_flyingtoasters/test_ad_flyingtoasters.c` | sim and sprite tests (no art needed) |
| `firmware/test/test_hacks/test_hacks.c` | expected name and baseline, under the `#ifdef` |
| `README.md`, `AGENTS.md` | the disk note and the archive link; the new pre-script and rules |

---

### Task 1: Disk image, resource fork and PICT decoding

**Files:**

- Create: `tools/after_dark_sprites.py`
- Test: `tools/tests/test_after_dark_sprites.py`

**Interfaces:**

- Produces, in `tools/after_dark_sprites.py`:
  - `strip_sector_tags(image: bytes) -> bytes`
  - `parse_resource_fork(fork: bytes) -> dict[tuple[str, int], bytes]`,
    keyed by (four-character type, id)
  - `decode_pict(data: bytes) -> tuple[int, int, list[tuple[int, int, int]]]`:
    width, height and row-major RGB888 pixels, drawn through `picFrame`
  - `class ExtractError(Exception)`

The test module builds its own inputs. Write helpers in it:
`make_tagged_image(volume: bytes) -> bytes`,
`make_resource_fork(resources: dict[tuple[str, int], bytes]) -> bytes`,
`make_pict_v1_bitmap(frame, bounds, rows)` and
`make_pict_v2_indexed(frame, clut, rows)`. Put PackBits run and literal
packets in both PICT helpers' rows.

- [ ] **Step 1: Write the failing tests**

```python
def test_strips_twelve_byte_tag_after_each_512_byte_sector():
    volume = bytes(range(256)) * 3200          # 819,200 bytes
    assert ads.strip_sector_tags(make_tagged_image(volume)) == volume

def test_plain_volume_passes_through():
    volume = bytes(819_200)
    assert ads.strip_sector_tags(volume) == volume

def test_rejects_image_of_unexpected_size():
    with pytest.raises(ads.ExtractError, match="838,400 or 819,200"):
        ads.strip_sector_tags(bytes(819_284))  # DiskCopy 4.2 sized

def test_resource_fork_round_trip():
    res = {("PICT", 128): b"abc", ("PICT", 228): b"defg", ("snd ", 130): b""}
    assert ads.parse_resource_fork(make_resource_fork(res)) == res

def test_v1_one_bit_pict_draws_through_picframe():
    # bounds 4 px wider on each side than the 8x2 frame; set bit = black
    w, h, px = ads.decode_pict(make_pict_v1_bitmap(...))
    assert (w, h) == (8, 2)
    assert px[0] == (0, 0, 0) and px[1] == (255, 255, 255)
    # a set bit in the 4 px outside the frame does not appear

def test_v2_four_bit_pict_maps_through_its_colour_table():
    clut = {0: (0, 0, 0), 5: (0xFFFF, 0x8000, 0x0000)}
    w, h, px = ads.decode_pict(make_pict_v2_indexed(...))
    assert px[3] == (255, 128, 0)              # 16-bit channel >> 8

def test_unknown_opcode_raises_with_its_value():
    with pytest.raises(ads.ExtractError, match="0x0031"):
        ads.decode_pict(...)
```

- [ ] **Step 2: Run them and see them fail**

Run:

```bash
uv run --with pytest --with pillow pytest tools/tests/test_after_dark_sprites.py -v
```

Expected: collection error, no module `after_dark_sprites`.

- [ ] **Step 3: Implement the three functions**

- **Tag stripping:** a 838,400-byte image is 1600 sectors of 524 bytes.
  Keep the first 512 bytes of each.
- **Resource fork:** standard Mac layout. Read the data and map offsets from
  the header, the type list at the map's `typeListOffset`, then 12-byte
  reference entries holding a 24-bit data offset. Each resource's data is a
  4-byte length followed by the bytes.
- **PICT opcodes:** v1 has one-byte opcodes. v2 starts `0011 02FF`, has
  two-byte opcodes and word-aligns each one. Handle exactly what the ten
  sprites use:
  - `0x11` version, `0x0C00` header (24 bytes)
  - `0x1E` DefHilite
  - `0x01` clip region (length-prefixed)
  - `0xA0` short comment (2 bytes)
  - `0xA1` long comment
  - `0x98` PackBitsRect
  - `0xFF` end
- **PackBitsRect:**
  - The high bit of `rowBytes` marks a PixMap. A PixMap has a 36-byte
    header after `bounds`, then a colour table: seed, flags, count, then
    entries of (index, r, g, b), with `flags & 0x8000` meaning use the
    position, not the stored index.
  - After the image header come srcRect, dstRect and mode.
  - Rows are unpacked when `rowBytes < 8`, otherwise PackBits with a byte
    count that is one byte when `rowBytes <= 250`, else two.
- **Output:** the frame-sized image. Pixel (x, y) reads bitmap column
  `frame.left + x - bounds.left` and row `frame.top + y - bounds.top`. In
  1-bit mode a set bit is black.

- [ ] **Step 4: Run them and see them pass**

Same command. Expected: all pass. Then
`ruff check tools/after_dark_sprites.py tools/tests/test_after_dark_sprites.py`
and `ruff format --check` on the same two files.

- [ ] **Step 5: Mutation-check and commit**

Break the `frame.left - bounds.left` offset (use 0), then the
`flags & 0x8000` branch. Watch a test fail for each, restore, and confirm
with `git diff`. Then:

```bash
git add tools/after_dark_sprites.py tools/tests/test_after_dark_sprites.py
git commit -m "After Dark sprites tool: tags, resource forks, PICTs"
```

### Task 2: Header writer and the command line

**Files:**

- Modify: `tools/after_dark_sprites.py`
- Test: `tools/tests/test_after_dark_sprites.py`

**Interfaces:**

- Consumes: Task 1's functions.
- Produces:
  - `mono_bits(pixels: list[tuple[int, int, int]]) -> bytes`: 64×64 →
    512 bytes, MSB first, 1 = black
  - `sprites_from_fork(fork: bytes) -> tuple[list[list[tuple[int, int, int]]], list[bytes]]`:
    five colour pixel lists (PICT 228, 229, 230, 231, 232) and five mono
    bitmaps (128, 129, 130, 131, 132), each checked to be 64×64
  - `render_header(colour, mono, sha256: str) -> str`
  - `read_module_fork(volume: bytes) -> bytes`, which imports `machfs`
    inside the function and reads
    `After Dark Files` / `Flying Toasters`
  - `main(argv: list[str] | None = None) -> int`:
    `uv run tools/after_dark_sprites.py IMAGE -o HEADER`
- The header, consumed by Task 6, guarded `AFTER_DARK_TOASTER_SPRITES_H`:

```c
/* Generated from <image name>, sha256 <hex>. Do not commit. */
#define AD_ART_SIZE 64
static const unsigned char ad_colour_rgb[5][64 * 64 * 3]; /* frames 0-3, toast */
static const unsigned char ad_mono_bits[5][64 * 64 / 8];  /* MSB first, 1 = black */
```

- The script's inline metadata lists `machfs` as its only dependency.

- [ ] **Step 1: Write the failing tests**

```python
def test_mono_bits_packs_msb_first_with_black_as_one():
    px = [(0, 0, 0)] + [(255, 255, 255)] * (64 * 64 - 1)
    assert ads.mono_bits(px)[:2] == bytes([0x80, 0x00])

def test_missing_pict_is_named_in_the_error():
    fork = make_resource_fork({("PICT", i): make_64px_pict() for i in
                               [128, 129, 130, 131, 132, 228, 229, 230, 231]})
    with pytest.raises(ads.ExtractError, match="PICT 232"):
        ads.sprites_from_fork(fork)

def test_wrong_sized_pict_is_rejected():
    ...  # one 32x32 PICT -> ExtractError naming its id and size

def test_header_declares_both_arrays_and_the_hash():
    text = ads.render_header(colour, mono, "ab12")
    assert "ad_colour_rgb[5][64 * 64 * 3]" in text and "ab12" in text

def test_main_writes_nothing_when_extraction_fails(tmp_path):
    out = tmp_path / "h.h"
    (tmp_path / "img").write_bytes(bytes(100))
    assert ads.main([str(tmp_path / "img"), "-o", str(out)]) != 0
    assert not out.exists()

def test_real_image_gives_ten_sprites():
    pytest.importorskip("machfs")
    image = REPO / "vendor" / "after-dark-v2.0v-800k"
    if not image.is_file():
        pytest.skip("no After Dark disk image")
    colour, mono = ads.sprites_from_fork(
        ads.read_module_fork(ads.strip_sector_tags(image.read_bytes())))
    assert [len(c) for c in colour] == [4096] * 5
    assert [len(m) for m in mono] == [512] * 5
```

- [ ] **Step 2: Run and see them fail**

Run:

```bash
uv run --with pytest --with pillow --with machfs \
  pytest tools/tests/test_after_dark_sprites.py -v
```

Expected: the new tests fail with `AttributeError`.

- [ ] **Step 3: Implement the five functions**

`main` writes through `HEADER.partial` and renames it. On `ExtractError` it
prints the message to stderr and returns 1.

- [ ] **Step 4: Run and see them pass**

Same command. With the image present, `test_real_image_gives_ten_sprites`
passes; without `--with machfs`, it is skipped. Then run the whole tools
suite, `uv run --with pytest --with pillow pytest tools/tests`, and ruff on
the touched files.

- [ ] **Step 5: Mutation-check and commit**

Make `mono_bits` LSB first and watch it fail. Restore. Then:

```bash
git add tools/after_dark_sprites.py tools/tests/test_after_dark_sprites.py
git commit -m "After Dark sprites tool: header writer and command line"
```

### Task 3: PlatformIO pre-script

**Files:**

- Create: `firmware/after_dark_sprites.py`
- Modify: `firmware/platformio.ini` (the `extra_scripts` line of
  `env:stopwatch`, `env:native` and `env:dump`: append
  `pre:after_dark_sprites.py`)

**Interfaces:**

- Produces:
  - `$BUILD_DIR/generated/after_dark/toaster_sprites.h`
  - `CPPPATH` += `$BUILD_DIR/generated`
  - `CPPDEFINES` += `HAVE_AFTER_DARK_TOASTERS` (only when the header was
    made)

Model it on `firmware/pacman_sprites.py`:

- Image path: `os.environ.get("AFTER_DARK_IMAGE")`, else
  `<repo>/vendor/after-dark-v2.0v-800k`. Relative paths are taken from the
  repo root.
- Image absent: print
  `ad-flyingtoasters: no After Dark disk image at <path>; left out` and
  return.
- Image present: regenerate when the header is missing or older than the
  image or the tool, by running
  `uv run tools/after_dark_sprites.py <image> -o <header>`. A non-zero exit
  is `sys.exit` with the tool's message.
- The define and include path are appended to both `env` and `projenv` if
  needed, so that test sources see the define. Step 2 checks this.

- [ ] **Step 1: Build without the image**

Run (from `firmware/`, worktree paths as in `AGENTS.md`):
`AFTER_DARK_IMAGE=/nonexistent pio run -e dump`
Expected: success, and the "left out" line in the output.

- [ ] **Step 2: Build with the image**

Run: `pio run -e dump`, then
`ls .pio/build/dump/generated/after_dark/toaster_sprites.h`.
Expected: the header exists and holds the image's SHA-256. Add a temporary
`#ifndef HAVE_AFTER_DARK_TOASTERS #error #endif` in a test file, run
`pio test -e native -f test_start_pick`, and see it compile. Remove the line.

- [ ] **Step 3: Build with a broken image**

Run: `AFTER_DARK_IMAGE=<scratchpad>/junk.img pio run -e dump`, with
`junk.img` holding 100 bytes.
Expected: the build fails with the tool's "838,400 or 819,200" message.

- [ ] **Step 4: Commit**

```bash
git add firmware/after_dark_sprites.py firmware/platformio.ini
git commit -m "Pre-script: extract After Dark sprites when the disk is present"
```

### Task 4: Simulation

**Files:**

- Create: `firmware/src/hacks/ad_flyingtoasters/ad_sim.h`, `ad_sim.c`
- Test: `firmware/test/test_ad_flyingtoasters/test_ad_flyingtoasters.c`

**Interfaces:**

- Produces (`ad_sim.h`):

```c
#define AD_MAX_FLYERS 20
#define AD_RECT 70   /* flyer rect: 64 art + 6 px strip */
#define AD_STRIP 6
typedef struct { int top, left, bottom, right; } AdRect;
typedef struct {
  int live, toast, speed, frame, flap_down;
  AdRect r;
} AdFlyer;
typedef struct {
  AdRect bounds;
  int n, running;
  AdFlyer f[AD_MAX_FLYERS];
  long (*rnd)(void);           /* random() in the hack; scripted in tests */
} AdSim;
/* sprite: 0-3 toaster frame, 4 toast. Called after the flyer's move. */
typedef void (*AdDrawFn)(void *ctx, const AdRect *r, int sprite);

void ad_sim_init(AdSim *, AdRect bounds, int n, long (*rnd)(void)); /* scatter inside */
void ad_sim_draw_all(AdSim *, AdDrawFn, void *ctx);  /* DoBlank: draw each once */
void ad_sim_step(AdSim *, AdDrawFn, void *ctx);      /* AdvanceFlyers */
int  ad_sim_collision(const AdSim *, int i, AdRect cand, int *dv, int *dh);
```

Everything follows the spec's "Flyers", "Motion" and "Animation"
pseudo-code:

- Random values are `labs(rnd() % k)`.
- `ad_sim_init` makes the first scatter with `running = 0`, then sets
  `running = 1`.
- Drawing calls the callback with the current frame, then advances the
  flap.
- `ad_sim_collision` returns 1 and sets the nudge, or returns 0.

Tests use a scripted `rnd` (a static array and an index) and a recording
draw callback.

- [ ] **Step 1: Write the failing tests**

- `test_flap_runs_0_1_2_3_2_1_0`: one toaster, 7 draws, frames recorded
  `{0,1,2,3,2,1,0}`.
- `test_speed_from_roll`: rolls 24, 25, 49, 50, 74 give speeds 1, 2, 2, 3,
  3; 75 and 99 give toast at speed 1.
- `test_step_moves_speed_times_minus2_plus1`: for speeds 1-3 alone on
  screen, the rect moves by (−2s, +s).
- `test_nudge_beside_drops_two_more`: an obstacle mostly to the side gives a
  net (−2, +3) for a speed-2 toaster.
- `test_nudge_above_goes_left_below_goes_less_left`: nets (−3, +1) and
  (−1, +1).
- `test_nudged_move_that_still_collides_keeps_base_step`: net (−2, +1).
- `test_speed_one_never_checks`: a speed-1 toaster moves into an overlap.
- `test_off_screen_flyer_is_removed_and_respawned_at_an_edge`: after it
  leaves, the next step puts its slot either at `bottom == bounds.top` (then
  moved one base step) or at `left == bounds.right`.
- `test_initial_scatter_is_inside_bounds`: 200 seeds, every rect inside.
- `test_slot_stays_empty_after_four_colliding_tries`: a scripted `rnd` that
  always lands on an existing flyer; after one step the slot is not live and
  exactly 4 placements were tried; the next step tries again.
- `test_trail_is_wiped`: on a 466×466 `uint16_t` test canvas, a test blitter
  copies a 70×70 stub sprite (black 6 px strip on the top rows and the right
  columns, white elsewhere) clipped to the canvas. After 300 steps of 10
  flyers, every non-black pixel lies inside some live flyer's rect.

- [ ] **Step 2: Run them and see them fail**

Run: `pio test -e native -f test_ad_flyingtoasters`
Expected: link errors for `ad_sim_*`.

- [ ] **Step 3: Implement `ad_sim.c`**

- [ ] **Step 4: Run them and see them pass**

Same command. Expected: all pass under ASan.

- [ ] **Step 5: Mutation-check and commit**

Make three separate mutations, watching a test fail for each:

- the ping-pong turn (`frame == 3` → `frame == 2`);
- the nudge axis swap;
- the try count (4 → 3).

Restore each and confirm with `git diff`. Then:

```bash
git add firmware/src/hacks/ad_flyingtoasters/ad_sim.h firmware/src/hacks/ad_flyingtoasters/ad_sim.c firmware/test/test_ad_flyingtoasters/test_ad_flyingtoasters.c
git commit -m "ad-flyingtoasters: simulation"
```

### Task 5: Sprite building, doneness and picks

**Files:**

- Create: `firmware/src/hacks/ad_flyingtoasters/ad_sprites.h`,
  `ad_sprites.c`
- Test: `firmware/test/test_ad_flyingtoasters/test_ad_flyingtoasters.c`

**Interfaces:**

- Produces (`ad_sprites.h`):

```c
typedef enum { AD_DONE_LIGHT, AD_DONE_MEDIUM, AD_DONE_DARK } AdDoneness;
typedef struct { int colour; AdDoneness doneness; } AdChoice;
AdChoice ad_choose(uint32_t r);                 /* bit 0: colour; (r >> 1) % 3: doneness */
void ad_apply_doneness(unsigned char *rgb, int pixels, AdDoneness);
void ad_expand_mono(const unsigned char bits[512], unsigned char rgb[64 * 64 * 3]);
/* 70x70 little-endian RGB565 (what a 16-bit XImage holds): art at x 0..63,
 * y 6..69; rows 0..5 and columns 64..69 black. */
void ad_build_sprite(const unsigned char rgb[64 * 64 * 3], uint16_t out[70 * 70]);
```

- **Doneness:** `c' = max(c - k, 0)` per channel. The spec's 16-bit
  constants, shifted right 8 bits, give k:
  - Medium (79, 79, 79)
  - Dark (127, 111, 111)
- **RGB565:** the usual 5-6-5 packing, not byte-swapped, as
  `tools/make_logo_blob.py`'s `rgb565`.

- [ ] **Step 1: Write the failing tests**

- `test_doneness_medium_and_dark_subtract_and_clamp`: (200, 150, 100)
  becomes (121, 71, 21) for Medium and (73, 39, 0) for Dark. Light leaves
  it unchanged. Black stays black.
- `test_mono_set_bit_is_black_clear_is_white`
- `test_sprite_has_black_strip_top_and_right`: a white art square gives
  rows 0-5 = 0, columns 64-69 = 0, and (0, 6) = 0xFFFF.
- `test_choose_covers_all_six`: `r` 0-5 give the six combinations, each
  once.

- [ ] **Step 2: Run them and see them fail**

Run: `pio test -e native -f test_ad_flyingtoasters`
Expected: link errors for `ad_*`.

- [ ] **Step 3: Implement `ad_sprites.c`**

- [ ] **Step 4: Run them and see them pass**

Same command.

- [ ] **Step 5: Mutation-check and commit**

Remove the clamp, then put the art at y 0 instead of 6, watching a test
fail each time. Restore. Then:

```bash
git add firmware/src/hacks/ad_flyingtoasters/ad_sprites.h firmware/src/hacks/ad_flyingtoasters/ad_sprites.c firmware/test/test_ad_flyingtoasters/test_ad_flyingtoasters.c
git commit -m "ad-flyingtoasters: sprites, doneness and picks"
```

### Task 6: The hack, registered

**Files:**

- Create: `firmware/src/hacks/ad_flyingtoasters/ad_flyingtoasters.c`
- Modify:
  - `firmware/src/hacks/registry.c`: an `extern` and the entry after
    `&hextrail_hack`, both under `#ifdef HAVE_AFTER_DARK_TOASTERS`
  - `firmware/test/test_hacks/test_hacks.c`:
    - `"ad-flyingtoasters"` at the end of `test_registry_lists_hacks_in_order`'s
      list
    - a `kBaseline` entry, under the same `#ifdef`
    - `test_blit_clips_at_every_edge`
    - `test_restart_does_not_leak`

**Interfaces:**

- Consumes: Tasks 2 (header), 4 and 5.
- Produces: `const HackEntry ad_flyingtoasters_hack`, with name
  `"ad-flyingtoasters"`, defaults `{".background: black", 0}`, and its init,
  draw and free functions.

Glue decisions:

- **Entropy:** `static uint32_t ad_entropy(void)` returns `random()` under
  `XSHIM_NATIVE` and `esp_random()` otherwise. The forced picks come from
  `AD_TOASTERS_ART` (0 = 1-bit, 1 = colour) and `AD_TOASTERS_DONENESS`
  (0, 1, 2) when defined. Any other value is an `#error`.
- **Init:**
  - Choose the art and doneness.
  - Copy the colour art and apply doneness to the toast only, or expand the
    mono bitmaps.
  - Build five sprites into 70×70 16-bit `ZPixmap` images
    (`XCreateImage` with `data` NULL, then pointed at a malloc'd buffer
    that the image owns).
  - `ad_sim_init` with bounds (0, 0, 466, 466) taken from the window size
    and n = 10, then `ad_sim_draw_all`.
  - Log `ad-flyingtoasters: <colour|1-bit>, doneness=<light|medium|dark>`
    with `fprintf(stderr, ...)`, as the shim logs.
- **Draw callback:** `XPutImage` at `(r->left, r->top)`.
- **Return:** `33333` (µs), aiming at 30 steps a second.
- **Free:** `XDestroyImage` on all five.
- **Credits:** the file's header comment names Jack Eastman, Bruce
  Burkhalter, Patrick Beard and Tomoya Ikeda, ©1990-91 Berkeley Systems.
  It says the art is read from the user's own disk at build time and links
  the Internet Archive copy.

- [ ] **Step 1: Write the failing tests (in `test_hacks.c`, under the `#ifdef`)**

- `test_blit_clips_at_every_edge`: with the hack running, put each sprite
  at x −69, −30, 430 and 465 and y −69, −30, 430 and 465 through the shim.
  The canvas outside the clipped sprite stays as it was: compare row by
  row, not just "no ASan error".
- `test_restart_does_not_leak`: 50 starts and stops, measured at rest as in
  the existing leak tests.
- Add the expected name and a `0` placeholder baseline.

- [ ] **Step 2: Run them and see them fail**

Run: `pio test -e native -f test_hacks`
Expected: the registry-order and count tests fail; the hack is missing.

- [ ] **Step 3: Implement the glue and register it**

- [ ] **Step 4: Run the suite**

Run: `pio test -e native`
Expected: all pass, including the every-hack 3000-frame run.

- **Baseline:** take the hack's pin with `HACK_PINS_OUT`. Dump frame 200
  (`pio run -e dump`, then `dump <last index> 200`, then
  `rgb565_to_png.py`) and look at it yourself first. Then put it in
  `kBaseline` as "taken after looking at the frames", and run again.
- **Build for the device:** `pio run -e stopwatch`.
- **Force flags:** with `AD_TOASTERS_ART=2`, the build must stop at the
  `#error`.
- **Without the image:** `AFTER_DARK_IMAGE=/nonexistent pio test -e native -f test_hacks`
  also passes.

- [ ] **Step 5: Commit**

```bash
git add firmware/src/hacks/ad_flyingtoasters/ad_flyingtoasters.c firmware/src/hacks/registry.c firmware/test/test_hacks/test_hacks.c
git commit -m "ad-flyingtoasters: register the hack"
```

### Task 7: Docs, tools and the device

**Files:**

- Modify: `README.md`, `AGENTS.md`
- Maybe modify: `tools/score_hacks.py`, `tools/speed_backtest.py`

- [ ] **Step 1: Scorer and backtest**

Run `uv run tools/score_hacks.py --no-probe --out <scratchpad>/a.md` and
`(cd firmware && pio run -e dump) && uv run tools/speed_backtest.py`. If
either breaks on `ad_flyingtoasters`, which is not an xscreensaver hack,
make it skip registered names it has no source for. Add a test to the
matching `tools/tests` file first.

- [ ] **Step 2: Docs**

- **README:** Flying Toasters needs your own After Dark 2.0 disk at
  `vendor/after-dark-v2.0v-800k` (or `AFTER_DARK_IMAGE`). Link
  <https://archive.org/details/after-dark-2.0v-800k>. Say it is left out
  without the disk.
- **`AGENTS.md`:**
  - the pre-script;
  - `HAVE_AFTER_DARK_TOASTERS`;
  - `AFTER_DARK_IMAGE`;
  - that the hack stays last in `g_hacks[]`;
  - the force flags;
  - that no extracted bytes may be committed.

Run `markdownlint README.md AGENTS.md` from the repo root.

- [ ] **Step 3: Commit**

```bash
git add README.md AGENTS.md
git commit -m "Docs: ad-flyingtoasters and the After Dark disk"
```

- [ ] **Step 4: On the device (ask Rod before flashing)**

- **Measure:** flash with `START_HACK="ad-flyingtoasters"` and
  `ROTATE_SECONDS=0`. Capture serial for 3 minutes. Report the mean and the
  lowest `fps=` window, and `rows=`.
- **Show Rod:** flash with the force flags in turn: colour with each
  doneness, then 1-bit. Ask him to check that the toast darkens from Light
  to Dark, and that the 1-bit art is white line art on black.
- **Afterwards:** say which build the board holds.

---

## Self-review

- **Spec coverage:**
  - extraction: Tasks 1-3;
  - behaviour: Task 4;
  - doneness and picks: Task 5;
  - the hack and registration: Task 6;
  - docs, scorer and device: Task 7;
  - sound and runtime settings are non-goals.
- **Proportion:** the plan names signatures, values and tests. The only
  bodies are the spec's algorithm, by reference.
