# How Mass Effect Infiltrator Bridge was built: a step-by-step account

Русская версия: [PORTING_GUIDE.md](PORTING_GUIDE.md).

Mass Effect Infiltrator 1.0.58 is an EA / IronMonkey Studios game: the EA Blast framework, the m3g renderer,
the library `libMassEffect.so` built for `armeabi`. Getting it to start on the phone was quick, because the
bridge and the wrapper had already been built for N.O.V.A. 3. After that, though, nearly every problem
turned out to be not in the port but in the game itself or in its data: touch codes that differ in every
build; a string table with checksums; a flaw in a sort comparison that made the game crash in combat.

Every step has the same shape: what was seen, how the cause was found, what was changed and where. The
current state is in [PROJECT_STATE.md](../PROJECT_STATE.md) (Russian), the summary in
[README.md](../README.md), and the original PortMaster port is described in [PORT.md](PORT.md).

Status: the game can be played through — menu, missions, combat, checkpoints; with the limit at 120,
combat runs at 98–120 fps (median 116) on a OnePlus 13s.

## Starting point

- The base is EapRules' port for PortMaster: a bionic ELF loader, a fake JNI and libc thunks
  (`src/`, `android/`, `loader/`, `jni/`, `thunks/`).
- The GLES bridge, the APK wrapper, qemu and the tools come from NOVA3 Bridge.
- Game data: the APK and a cache archive (~740 MB unpacked). The SHA1 of `libMassEffect.so` was checked
  against the one the port was written for.

## Part 1. Moving the bridge over

### 1.1. The game turned out to be GLES2, not GLES1

- **What was written:** the port's README and comments said the game uses GLES1.
- **How it was checked:** from the library's imports — 177 `gl*` functions, 64 of them shader ones.
- **Conclusion:** that description belonged to a sibling port. The N.O.V.A. 3 bridge (GLES2) fitted almost
  unchanged.
- **Rule:** do not trust someone else's description until you have looked at the imports.

### 1.2. Missing functions

Calls that N.O.V.A. 3 did not need were added to the bridge: `glBindAttribLocation` (a new op),
`glUniform2/3/4` for float and int, `glUniformMatrix2/3`, `glVertexAttrib*`, `glStencil*Separate`, `glHint`,
`glFinish`, `glDetachShader`, `glValidateProgram`, `glCopyTexImage2D`, `glTexParameter*v`, `glIs*`, `glGet*`.
The generator is [gen_glbridge.py](../apk/tools/gen_glbridge.py), the game side is
[gl_bridge.cpp](../src/gl_bridge.cpp).

### 1.3. First run

On the phone from the first install: qemu starts, the bridge works, the main menu, ~60 fps. Then came the
steps below.

## Part 2. Touch: the codes depend on the build

- **Seen:** touches reach the game but nothing happens.
- **How it was found:** the game takes touches through the export `NativeOnPointerEvent(env, obj, rawEvent,
  module, id, x, y)`. The `rawEvent` values are not universal: three exports return them,
  `NativeGetIdRawPointerDown/Move/Up`. The port was sending another build's values and the engine silently
  dropped them.

```sh
readelf --dyn-syms -W libMassEffect.so | grep NativeGetIdRawPointer
arm-linux-gnueabihf-objdump -d --start-address=0x35ae94 --stop-address=0x35aea0 libMassEffect.so
```

  Each of the three functions is three instructions: `mov r0,#N; add r0,r0,#14; bx lr`.
- **Values for 1.0.58:** down `0x6000e`, move `0x4000e`, up `0x8000e`.
- **Fix:** [input_bridge.cpp](../android/input_bridge.cpp). In addition `MASSEFFECT_TOUCH=1` tells the
  engine that a touch screen is present.
- **Rule:** for another build of the game the values are worked out again with the same command. Do not
  take the constant from someone else's port.

**Debugging without a finger.** The app queues the lines of an `inject.txt` file next to the log once:
`touch 1 x y id`, `touch 0 x y id`, `button a down|up`, `sleep <ms>`. A screenshot is
`adb shell screencap -p`.

## Part 3. Picture and text

### 3.1. Dark silhouettes instead of textures

- **Seen:** models are black in 3D.
- **How it was found:** the texture-upload wrappers in [symtab_glprobe.cpp](../src/symtab_glprobe.cpp)
  looked up `glGetString`, `glTexImage2D` and `glCompressedTexImage2D` through `SDL_GL_GetProcAddress`. In
  bridge mode there is no SDL context, so the functions came back NULL. DXT/ATC support was taken to be
  absent and the uploads were lost without a message.
- **Fix:** `#include "gl_bridge.h"` in that file: a macro redirects the lookup to the bridge.
- **Rule:** any port file that looks GL functions up through SDL must include `gl_bridge.h`.

### 3.2. Boxes instead of Cyrillic

- **Cause:** the engine opens the system font `/system/fonts/DroidSans.ttf`. The handheld port substituted
  the game's own font, which has no Cyrillic.
- **Fix:** Android has real system fonts. `MASSEFFECT_SYSTEM_FONT` points at `RobotoStatic-Regular.ttf` and
  the substitution is turned off.

### 3.3. Empty "Yes" and "No" buttons

- **Seen:** the buttons of confirmation dialogs have no text.
- **Cause:** a Russian patch sits on top of the English string table and leaves the keys `NO` and `YES`
  without values.
- **Fix:** [StringsFix.java](../apk/src/com/jamescrumble/meinfiltrator/StringsFix.java) adds the two
  strings to the table on every start.
- **How the table is laid out** (`strings/ENG_US/masseffect.bin`, format SBIN v3): the `DATA` chunk holds
  "key, value index" pairs; `CHDR` is an array of "offset, length"; `CDAT` is the strings themselves.
- **The main trap:** each chunk's header stores an FNV-1 hash of its contents. On a mismatch the engine
  rejects the whole table and every string turns into `XXXXX[KEY]XXXXX`. The first two attempts at editing
  without rehashing broke all the strings — including an edit of just three bytes.
- **Check:** the same algorithm was written in Python, and the Java and Python results were compared byte
  for byte.

The text `XXXXX[STRING_TAPTOAIM_WARNING]XXXXX` is not a port bug: that key is in none of the game's tables.

## Part 4. The crash in combat: an engine defect

- **Seen:** in the 18th minute, in combat — SIGSEGV in `m3g::Renderer::RenderRenderNode`.
- **Picture:** the render list held an `m3g::Appearance` object where an `m3g::Node` should be; the virtual
  call landed on type data.
- **Hypotheses:** a use of an already freed node, or the list being corrupted during sorting.
- **How it was found:** the comparison function `SortedRenderNode::operator<` is **not transitive**. One of
  its steps — "if both elements have the flag set, compare by vertex buffer" — depends on the pair, not on
  the element. `eastl::quick_sort` on such an order runs past the end of the array and drags stale entries
  from earlier frames into the list.
- **How it was proved:** the game's own code (a copy of the bytes from the library) was run on its own
  under qemu-arm on random data. 11 of 20,000 sorts wrote beyond the list, by up to 90 entries. With the
  corrected comparison — none. Pairs with an equal flag compare as before (checked on 2 million pairs).
- **Fix:** [render_sort_patch.cpp](../src/render_sort_patch.cpp). `MASSEFFECT_NO_SORT_FIX=1` restores the
  engine's comparison.
- **What is not proved:** that this is what caused that particular crash. It is an inference from the
  crash picture; the direct check is long play without a crash. The NaN hypothesis was dropped: the
  comparison is on integers.

## Part 5. Speed

### 5.1. Matrix multiplication

- **Combat profile:** four `Vectormath::Aos::Matrix4` functions — multiply by vector, `transform`, matrix
  multiply and `*=` — took about 27 % of the samples.
- **Fix:** [neon_math_patch.cpp](../src/neon_math_patch.cpp) replaces their bodies with NEON code that
  keeps the same order of operations. The result is bit-exact on ordinary numbers: a test against the
  original bytes, 400 thousand rounds.
- **Particularity:** the gain depends on the CPU underneath the game. So at start the port measures the
  original and the replacement itself and installs only what is faster by a 15 % margin. The result is the
  `neon math:` lines in the log.
- **Measured on the phone:** matrix multiply 6.00 → 2.07 ms per 20,000 calls; multiply by vector — about
  even.

### 5.2. qemu: NEON without switching FPCR

- **Cause of the remainder:** in the qemu of the time a switch between VFP and NEON cost a write to the
  FPCR register.
- **Fix:** variant E — NEON f32 is computed under the ordinary FPCR; elements with denormals or NaNs take
  the slow path. The patch is `qemu/patches/neon_nofpcr.patch`. Accuracy was checked with the `fptest` and
  `neontest` tests against strict mode. Later replaced by variant F from Shadowgun Bridge.

### 5.3. The engine's frame limiter

- **Seen:** the game holds ~61 fps whatever the limit in the settings.
- **How it was found:** `EA::Blast::System::CreateFpsLimiter` creates the limiter with the constant 60
  (`mov r1, #60` at offset `+0x35c1d0`).
- **Fix:** `patch_fps_limit` in [patch.cpp](../src/patch.cpp) replaces the constant with the value of
  `MASSEFFECT_TARGET_FPS`. With the limit at 120 the menu runs at 117–120 fps.
- **Not checked:** combat at 30 and 90; whether the speed of the game itself depends on the frame rate.

### 5.4. Cores

Measured in combat with the limit at 120: on the middle cores 75–110 fps with dips to 50–70; on cores 6–7
with two frames in flight — minimum 98, median 116. On the fast cores ~117 even with the frequency ceiling
at 1209 MHz. Pinning to cores 6–7 is on by default.

### 5.5. Small things found with the system-call counter

- glibc opened `/etc/localtime` about 150 times a second. The `TZ` variable is now set from the phone's
  time zone: 1473 → 7–13 opens per 10 seconds.
- One trace line was printed every frame: 39 thousand of the 47 thousand lines in the log. It is now
  printed four times.

## Part 6. Audio

- **Seen:** audio lags, the delay growing to ~0.75 s.
- **Cause:** SDL's `disk` driver paces itself with a sleep that does not equal the period and runs slightly
  faster than real time. The surplus piles up in the pipe and the `AudioTrack` buffer.
- **Fix:** the speaker sets the pace. The sleep is shorter than the period (`SDL_DISKAUDIODELAY=15`), the
  pipe is cut to one period (4096 bytes), the track buffer is minimal.

## Rules that proved useful

1. **Check other people's claims against the binary**: GLES1 versus GLES2, the touch codes.
2. **Constants the game itself returns should be read from the game**, afresh for every build.
3. **A data format with checksums** breaks on any edit that does not rehash.
4. **A suspected engine defect should be proved separately**: lift the code out, run it on random data,
   compare with your fix.
5. **Install a code replacement by measuring on the spot** when the gain depends on the CPU.

## What is left

- "Oversensitive" running left and right on a large screen. Working hypothesis: the engine's thresholds
  are set in pixels for screens 480–540 high. Knobs to test with: `resolution.txt`,
  `MASSEFFECT_SCREEN_DPI`.
- The system's frequency limiting — as in the other projects.
- On-screen gamepad buttons.
