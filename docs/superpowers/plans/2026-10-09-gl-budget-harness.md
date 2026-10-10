# GL Budget Harness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use
> checkbox (`- [ ]`) syntax for tracking.

**Goal:** A standalone host tool that runs a GL hack through TinyGL, counts
what each frame asks of the rasteriser, and predicts the device frame rate, so
`cubicgrid` and `morph3d` can be compared before either is ported.

**Architecture:** `tools/gl_budget.py` builds a throwaway host program from the
shim sources, the glshim layer, TinyGL patched by `tools/tinygl_patch.py` plus
a few counter increments, one hack, and `firmware/native/gl_budget_main.c`. The
program runs N frames and prints per-frame counts as JSON. The Python side
turns the counts into a predicted frame time with the issue #43 cost model,
calibrated against Gears. Counting happens inside TinyGL, so display-list
replays are counted as drawn.

**Tech Stack:** Python 3.13 with `uv` and pytest, C (host `cc`), TinyGL as
patched at build time. No new dependencies.

**Spec:** Agreed in chat on 2026-10-09 (Rod: usable frame rate is the metric,
a cheap proxy is fine, standalone tool in `tools/`). Background and the
candidate list are in issue #45; the cost model is in issue #43.

## Global Constraints

- Python 3.13+, `uv`, ruff formatting, pytest. Markdown goes through the
  `/markdown` skill and `markdownlint` from the repo root.
- Copied hacks stay byte-identical. The tool reads `cubicgrid` and `morph3d`
  from `vendor/xscreensaver-6.16/hacks/glx/` (git-ignored) and never copies
  them into the repo. It reads Gears from `firmware/src/hacks/gears/`.
- `firmware/src/tinygl/` stays byte-identical. Counters are added only to the
  harness's temporary copy, never to `tools/tinygl_patch.py` or the firmware
  builds.
- Do not flash or touch the device. This work is host-only.
- Changing what every hack sees is not allowed without asking Rod. If a
  candidate hack needs a shim gap filled, stop and ask.
- Cost model constants (issue #43): clear 24.6 ms, display push 32 ms, setup
  about 4 us per vertex plus 2.8 us per lit vertex, fill about 40 ms for the
  heavy Gears scene (17,706 vertices, 17,620 triangles).
- Run `pio`/`uv` commands one at a time. Set `NO_COLOR=1` on output you parse.
- Commits end with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`
  and use named paths with `git add`.

## Review Focus

- A hack that draws through a display list built once and replayed each
  frame (`cubicgrid`, `deepstars`): the counts must be per frame, not zero
  after frame 1.
- A hack that draws lines or points rather than triangles (`cubicgrid`): they
  must show up in the report, not as an empty frame.
- A hack that does not build against the shim: report the missing symbols or
  the first compiler error and exit non-zero; never a Python traceback.
- A hack that never calls `init_GL`, or draws nothing: counts are zero and the
  report says so, rather than predicting the fixed cost as a good frame rate.
- Two runs with the same seed give identical counts, so a comparison is
  repeatable.

---

### Task 1: The cost model

**Files:**

- Create: `tools/gl_budget.py`
- Test: `tools/tests/test_gl_budget.py`

**Interfaces:**

- Produces: `FrameCounts` (a `dataclass` with `vertices`, `lit_vertices`,
  `triangles`, `lines`, `points`, `pixels`, all `float`, per frame averages);
  `MODEL` (a `dict[str, float]` with keys `clear_ms`, `push_ms`,
  `vertex_us`, `lit_vertex_us`, `pixel_ns`); `predict_ms(counts: FrameCounts,
  model: dict[str, float] = MODEL) -> float`; `fps(ms: float) -> float`.

- [ ] **Step 1: Write the failing tests** in `tools/tests/test_gl_budget.py`,
  importing `gl_budget as gb` the way `test_probe_hacks.py` imports its
  module:
  - `test_predict_with_nothing_drawn_is_the_fixed_cost`:
    `predict_ms(FrameCounts(0,0,0,0,0,0))` equals `clear_ms + push_ms` from
    `MODEL`.
  - `test_each_term_adds_its_own_cost`: with a model of
    `{clear_ms: 0, push_ms: 0, vertex_us: 4, lit_vertex_us: 2, pixel_ns: 10}`,
    1,000 vertices of which 500 lit and 100,000 pixels predict
    `4.0 + 1.0 + 1.0 = 6.0` ms.
  - `test_fps_is_the_inverse_of_milliseconds`: `fps(125.0) == 8.0`.
- [ ] **Step 2: Run** `uv run --with pytest pytest tools/tests/test_gl_budget.py
  -q`. Expected: FAIL, no module `gl_budget`.
- [ ] **Step 3: Implement** the names above in `tools/gl_budget.py`. `MODEL`
  starts with the Global Constraints values and a provisional `pixel_ns` of
  `40e6 / 1.0e6` (a placeholder that Task 5 replaces). Module docstring: say
  what the tool is for and that it predicts, it does not measure.
- [ ] **Step 4: Run** the same command. Expected: PASS.
- [ ] **Step 5: Commit** `tools/gl_budget.py tools/tests/test_gl_budget.py`
  with message "Add the GL budget cost model".

### Task 2: Counting inside TinyGL

**Files:**

- Modify: `tools/gl_budget.py`
- Test: `tools/tests/test_gl_budget.py`

**Interfaces:**

- Consumes: `tinygl_patch.patch_tree(sources) -> dict[str, str]` (paths
  relative to `firmware/src/tinygl`).
- Produces: `instrument(sources: Mapping[str, str]) -> dict[str, str]`, which
  takes the already patched tree and adds the counters; `COUNTER_NAMES`
  (a `tuple[str, ...]`: `vertices`, `lit_vertices`, `triangles`, `lines`,
  `points`, `pixels`); `load_tinygl(root: Path) -> dict[str, str]`, which
  reads `firmware/src/tinygl/{src,include}` the way `firmware/patch_tinygl.py`
  does and returns `instrument(patch_tree(...))`.

The patched TinyGL declares `extern double gl_budget_counts[6];` in
`src/zgl.h` and increments by index from `COUNTER_NAMES`. The C driver (Task
3) defines it.

- [ ] **Step 1: Write the failing tests:**
  - `test_instrument_adds_a_counter_at_each_site`: run `load_tinygl(ROOT)`
    and assert the output text for `src/vertex.c` contains
    `gl_budget_counts[0]` (in `glopVertex`) and `src/clip.c` contains
    counters for lines, points and triangles.
  - `test_instrument_fails_when_an_anchor_is_missing`: pass a tree with
    `src/clip.c` emptied; expect `ValueError` matching "has upstream
    changed?".
  - `test_instrument_leaves_the_source_tree_alone`: reading the tree twice
    gives identical input dicts (`instrument` must not mutate its argument).
- [ ] **Step 2: Run** and confirm all three fail for the right reason.
- [ ] **Step 3: Implement** `instrument` with the same anchor-and-count
  discipline as `tinygl_patch.py` (each anchor must occur exactly the expected
  number of times). Sites: first line of `glopVertex` (vertices; and lit
  vertices when `c->lighting_enabled`), first line of `gl_draw_point`,
  `gl_draw_line`, and the rasterised-triangle path. For triangles, replace
  each `c->draw_triangle_front(p0, p1, p2);` and
  `c->draw_triangle_back(p0, p1, p2);` in `gl_draw_triangle` with a call to a
  small static inline `gl_budget_triangle(p0, p1, p2)` (added at the top of
  `clip.c`) that increments `triangles`, adds half the absolute cross product
  of the `zp` screen coordinates to `pixels`, clamped to the framebuffer
  area, then continues to the original call.
- [ ] **Step 4: Run** and confirm PASS. No `pio` run is needed: nothing in
  `firmware/` changes in this task.
- [ ] **Step 5: Commit** with message "Count vertices, triangles and fill
  inside a private copy of TinyGL".

### Task 3: Build and run one hack

**Files:**

- Create: `firmware/native/gl_budget_main.c`
- Modify: `tools/gl_budget.py`
- Test: `tools/tests/test_gl_budget.py`

**Interfaces:**

- Consumes: `probe_hacks.module_entry`, `probe_hacks.registry_source`,
  `probe_hacks.shim_sources`, `probe_hacks.undefined_symbols`,
  `probe_hacks.BUILD_FLAGS`, and Task 2's `load_tinygl`.
- Produces: `measure(name: str, source: str, root: Path, frames: int = 300,
  seed: int = 1, cc: str = "cc") -> dict`, returning
  `{"counts": FrameCounts, "host_ms": float, "frames": int}` or
  `{"error": str}`; and the C driver's output contract: one line per run,
  `counts frames=<n> vertices=<f> lit_vertices=<f> triangles=<f> lines=<f>
  points=<f> pixels=<f> host_ms=<f>`, means per frame after a warm-up of 10
  frames.

The C driver mirrors `firmware/native/dump_main.c`: creates a 466 by 466
canvas, `runner_create`, `runner_start(r, 0)`, `srandom(seed)` before it,
steps the warm-up frames, zeroes `gl_budget_counts`, steps `frames` frames,
prints the means. It defines `double gl_budget_counts[6]`.

`measure` writes the hack source in a temp directory as `<name>.c` prefixed by
`#define USE_GL` (as `firmware/src/hacks/gears_gl.c` does), writes the
patched TinyGL under `generated/tinygl/` with `generated/tinygl_unity.c`
from `tinygl_patch.unity_source`, and compiles: the hack, the registry,
`shim_sources` minus `dump_main.c` plus `gl_budget_main.c`, the files in
`firmware/src/glshim/` (including the `glx_*.c` helper wrappers), with
`-I` for `firmware/src`, `x11shim/include`, `xs_support`, `xs_support/glx`,
the generated directory, and `-DSTANDALONE -DHAVE_MOBILE -DXSHIM_NATIVE`.
Compile and run time limits as in `probe_hacks` (120 s).

- [ ] **Step 1: Write the failing tests**, each using a small stub hack
  written as a string in the test (an `XSCREENSAVER_MODULE`-style hack on the
  xlockmore macros; copy the shape of `firmware/src/hacks/gears/gears.c`'s
  entry points but keep it to a draw function that calls `init_GL` once):
  - `test_triangles_in_a_replayed_display_list_count_every_frame`: the stub
    builds a list of 10 triangles once and `glCallList`s it each frame. With
    30 frames, `counts.triangles == 10` and `counts.vertices == 30`.
  - `test_lines_and_points_are_counted`: 4 `GL_LINES` vertices and 3 points
    per frame give `lines == 2`, `points == 3`.
  - `test_lit_vertices_are_counted_only_when_lighting_is_on`: the same
    triangle with and without `glEnable(GL_LIGHTING)`.
  - `test_a_hack_that_draws_nothing_reports_zero`.
  - `test_a_broken_hack_returns_an_error_not_a_traceback`: source with a
    syntax error returns `{"error": ...}` whose text contains "error:".
  - `test_the_same_seed_gives_the_same_counts`: a stub that draws a
    `random()`-dependent number of triangles; two runs with seed 3 are equal,
    seed 4 differs.
  All marked `needs_cc` like `test_probe_hacks.py`.
- [ ] **Step 2: Run** and confirm they fail (no `measure`).
- [ ] **Step 3: Implement** `gl_budget_main.c` and `measure`. Use
  `probe_hacks.registry_source(class_name, prefix, xlockmore)` for the
  one-hack registry.
- [ ] **Step 4: Run** the tests; PASS. Mutation-check: change the counter in
  the replayed-list path so it counts only the list's first build and watch
  `test_triangles_in_a_replayed_display_list_count_every_frame` fail, then
  restore and confirm with `git diff`.
- [ ] **Step 5: Commit** "Run a GL hack on the host and count per frame".

### Task 4: Command line and report

**Files:**

- Modify: `tools/gl_budget.py`
- Test: `tools/tests/test_gl_budget.py`

**Interfaces:**

- Consumes: `measure`, `predict_ms`, `fps`.
- Produces: `hack_path(root: Path, name: str) -> Path` (Gears from
  `firmware/src/hacks/gears/gears.c`, anything else from
  `vendor/xscreensaver-6.16/hacks/glx/<name>.c`; raises `FileNotFoundError`
  naming the path); `render_report(rows: list[dict]) -> str` (a markdown
  table: hack, seed, vertices, lit, triangles, lines, points, pixels,
  predicted ms, predicted fps); `main(argv: list[str]) -> int`. CLI:
  `uv run tools/gl_budget.py [--frames N] [--seeds a,b,c] name [name...]`;
  a name may be `gears:7` to give that hack a single seed. Exit 1 if any hack
  errored, after printing the rest.

- [ ] **Step 1: Write the failing tests:** `hack_path` for Gears and for a
  missing vendor hack; `render_report` for one measured row and one error row
  (the error row shows its message instead of numbers, no `inf` fps); a
  zero-count row says "draws nothing" rather than predicting an fps.
- [ ] **Step 2: Run** and confirm FAIL.
- [ ] **Step 3: Implement** the three functions. Keep seeds default to
  `1,2,3`. The report ends with one line stating the model constants and that
  the figures are predictions (Global Constraints values).
- [ ] **Step 4: Run** the tests; PASS. Run `uv run tools/gl_budget.py
  gears:1` for real and read the output.
- [ ] **Step 5: Commit** "Add the gl_budget command line and report".

### Task 5: Calibrate against Gears

**Files:**

- Modify: `tools/gl_budget.py`
- Test: `tools/tests/test_gl_budget.py`

The two measured Gears points from issue #43 are the check: a heavy planetary
layout (17,706 vertices and 17,620 triangles, about 207 ms by the issue's
breakdown) and light layouts (about 4,300 vertices, about 9 fps, 111 ms).

- [ ] **Step 1: Find the seeds.** Run `gl_budget.py gears --seeds 1,...,40`
  and pick one seed whose counts are within 5 percent of the heavy scene
  (vertices near 17,706) and one near 4,300 vertices. Record both in a
  constant `GEARS_SEEDS = {"heavy": n, "light": m}` with a comment that they
  are seeds of this repo's Gears, not the spike's.
- [ ] **Step 2: Write the failing tests:**
  `test_model_reproduces_heavy_gears` (predicted within 10 percent of 207 ms)
  and `test_model_predicts_light_gears` (within 20 percent of 111 ms). The
  heavy point is used to fit `pixel_ns`; the light point is the held-out
  check, so say that in the test docstring.
- [ ] **Step 3: Fit `MODEL["pixel_ns"]`** so the heavy scene's fill term is
  40 ms: `pixel_ns = 40e6 / heavy_pixels`, rounded to two significant figures.
  If the light check misses by more than 20 percent, do not tune further:
  stop and report the miss to Rod with the numbers.
- [ ] **Step 4: Run** the tests; PASS. Mutation-check: halve `pixel_ns` and
  watch the heavy test fail; restore and confirm with `git diff`.
- [ ] **Step 5: Commit** "Calibrate the GL cost model against Gears".

### Task 6: Compare cubicgrid and morph3d

**Files:**

- Create: `docs/gl-budget.md`
- Modify: `AGENTS.md` (the Commands list), `.claude/rules/gl.md`

- [ ] **Step 1: Run** `uv run tools/gl_budget.py gears:<heavy> gears:<light>
  cubicgrid morph3d` and save the output.
- [ ] **Step 2: If either hack fails to build**, record the first error or the
  missing symbols. Fix only what the shim can absorb without changing what
  other hacks see; otherwise stop and ask Rod. Say which in the write-up.
- [ ] **Step 3: Write `docs/gl-budget.md`** (invoke the `/markdown` skill
  first): what the tool does and does not claim, the table with Gears next to
  the two candidates, and a plain verdict on which port to do first and why.
  State the proxy's known limits: no PSRAM contention, no real lighting cost
  per material, host `random()` layouts.
- [ ] **Step 4: Add the command** to the Commands list in `AGENTS.md` (one
  line) and a gotcha to `.claude/rules/gl.md` (counters live only in the
  harness's copy of TinyGL).
- [ ] **Step 5: Verify** `uv run --with pytest --with pillow pytest
  tools/tests`, `markdownlint docs/gl-budget.md AGENTS.md
  .claude/rules/gl.md`, `uvx ruff format --check tools/gl_budget.py
  tools/tests/test_gl_budget.py`. Report each separately.
- [ ] **Step 6: Commit** "Compare cubicgrid and morph3d with the GL budget
  tool". Remove `docs/superpowers/plans/` before any merge to `main`.
