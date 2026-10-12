# Mass Effect Infiltrator — native ARM port

Runs the 2011 EA/IronMonkey **Mass Effect Infiltrator mobile** game on Linux/ARM handhelds by
loading its original Android native library into a bionic/JNI compatibility
layer. No emulator or Android runtime is involved.

**Port and project by [EapRules](https://github.com/EapRules).**

**Android bridge by [JamesCrumble](https://github.com/JamesCrumble).** This repository adds a way to run the
port on 64-bit Android phones that have no AArch32 support: an APK wrapper (`apk/`), a patched `qemu-arm`
(`qemu/`) that executes the ARM32 port, and a GLES bridge (`src/gl_bridge.*`, `apk/src/.../GlBridge.java`)
that forwards the game's OpenGL ES calls to the phone's GPU. The port itself - loader, JNI layer, patches,
PortMaster packaging - is EapRules' work and is described below as he wrote it.

This directory targets the **v1.0.58 gamepad** build:

```text
lib/armeabi/libMassEffect.so
SHA1 ea58b733d3d267ab639431b50539542faa43f0d0
```

## Current status

The immutable harness reaches **M7/7** under qemu-arm + llvmpipe. The latest
run, with real-time dummy audio consumption and the ARMv8 VFP compatibility
patch enabled, reported:

```text
570 frames
84 successful content opens
151 texture uploads
35,511 draw calls
non-black framebuffer
11 synthetic JNI keys
4 measured scene changes
```

That proves startup, content loading, rendering and input-driven progression in
the harness. The PVRTC fallback is also confirmed on the real R36S Mali-G31:
the previously white characters, objects and backgrounds now render correctly.
The current candidate also initializes SDL audio, uses a bounded hardware
period and expands the obsolete VFP short-vector mixer operations for ARMv8.
Audio output, the new cursor/camera behavior, L2/R2 accelerometer gestures,
saves and a complete play-through still require device testing.

## Bring your own game

This repository and its packages contain no EA binary or asset. The supported
donor is the **Mass Effect Infiltrator v1.0.58 gamepad build**, Android package
`com.ea.games.meinfiltrator_gamepad`. Local developer runs give the loader an
already-extracted directory with:

```text
masseffect/
├── assets/
│   ├── EAMCore.ini
│   └── published/
└── lib/
    └── armeabi/
        └── libMassEffect.so
```

Run locally as:

```bash
./build/masseffect /path/to/extracted/masseffect
```

The PortMaster release also carries `eapx`, a content-based transactional
first-boot extractor. Release users may place the supported APK, ZIP or an
extracted folder in `ports/masseffect/`; its filename does not matter. eapx
stages and validates the complete payload before publishing it. The native
library is validated by critical regions, not a whole-file hash, so any
legitimate v1.0.58 gamepad-build copy is accepted; the launcher additionally
size-checks the `.so`. A complete game tree and reduced Vita-ready donors that
keep the campaign are both supported.

## Build and verify

```bash
docker run --rm -v "$PWD":/src -w /src masseffect-build make -j4
timeout 400 harness/verify.sh
```

`harness/verify.sh` is the read-only arbiter. Do not modify it or lower its
thresholds.

To collect the redistributable ARM dependencies and make the game-data-free
PortMaster zip:

```bash
docker run --rm -v "$PWD":/src -w /src masseffect-build make libs
./package_portmaster.sh
```

The zip is written to `build/masseffect-portmaster.zip`. It intentionally
contains neither `libMassEffect.so` nor `assets/published`.

## PortMaster install

Use PortMaster's standard independent-autoinstall workflow:

1. put our `masseffect-portmaster.zip` in PortMaster's `autoinstall/` directory
   without renaming it;
2. put the user's v1.0.58 gamepad-build APK/ZIP in `ports/masseffect/` (or an
   extracted donor under `ports/masseffect/gamedata/`);
3. open PortMaster and wait for the exact **Finished running autoinstall**
   dialog; acknowledge it and let PortMaster return or close normally;
4. reboot through the firmware menu, then launch Mass Effect Infiltrator from Ports. Do not
   hard-power the console while PortMaster is still installing.

The first launch extracts and validates the donor automatically. Complete
CFW-specific paths, the two autoinstall gotchas and accepted donor layouts are
in [`ports/masseffect/README.md`](ports/masseffect/README.md). The resulting
layout is:

```text
ports/
├── Mass Effect Infiltrator.sh
└── masseffect/
    ├── masseffect
    ├── masseffect.gptk
    ├── eapx.py
    ├── masseffect.eapx.json
    ├── libs.armhf/
    ├── assets/                # your copy
    ├── lib/armeabi/           # your copy
    └── var/                   # saves/settings
```

The launcher follows PortMaster's `directory` variable, so it works from
either `/roms` or `/roms2`. It also refreshes only Mass Effect Infiltrator's normalized
ArkOS artwork at `ports/images/Mass Effect Infiltrator.png` when a direct update left an old
APK icon cached there; the canonical portable cover remains
`masseffect/cover.png` as required by PortMaster's `gameinfo.xml` format.

## Controls

This binary has no `AInputQueue` imports. Input is delivered through its
exported JNI entry points, matching the working Vita port:

- title/menus: D-pad moves a visible software cursor, A taps it
- L3 or R3 toggles the menu cursor after it has been dismissed
- Start restores the cursor while opening the pause menu
- L2 simulates the accelerometer tilt used to rotate/switch weapon fire mode
- R2 simulates the accelerometer motion required for a Zero-G jump
- buttons outside cursor mode → `KeyboardAndroid.NativeOnKeyDown/Up`
- left stick → virtual touchscreen movement stick
- right stick → virtual touchpad aiming stick

The original game's menus do not support gamepad navigation; the Vita port
uses its physical touchscreen. The cursor is therefore a required input
bridge on non-touch PortMaster handhelds, not optional decoration. Moving
either analog stick dismisses it so the same controls can drive gameplay.

The pointer callback uses base AAPCS because the game is softfp and the loader
is hardfp.

## Graphics and real-device status

The first `d4ca229` build was tested on an R36S with its Mali-G31 driver:

- the image is correctly centred at 640x480;
- the D-pad cursor and physical controls work and can advance through menus;
- menu UI is visible;
- 3D characters, objects and backgrounds render mostly white, sometimes with
  only an edge, shadow or silhouette visible;
- audio is not working yet.

The interactive emulator reproduced that exact visual failure. Per-call GL
diagnostics identified rejected `glCompressedTexImage2D` uploads:
`0x8c00/0x8c02` are PVRTC1 4bpp RGB/RGBA, formats unsupported by both llvmpipe
and Mali-G31. The loader now uses Imagination's MIT-licensed decoder and
uploads RGBA8888 when the driver does not advertise native PVRTC.

A subsequent local capture rendered the complete menu environment with its
textures, lighting and materials, and the immutable harness remained M7/7. The
candidate with SHA-256
`9199544a9db9113e20facac61fb518dfc892beff35f17156ff3e313924a015da`
was then tested on the real R36S and the user confirmed that the full 3D scene
also renders correctly there.

That hardware pass exposed two isolated input issues in the otherwise working
controls: the provisional cross cursor could not be recovered after analog
input, and a held right stick produced only one finite camera gesture. The next
candidate replaces the cross with a high-contrast arrow, restores it with
L3/R3 or Start, refreshes sticks every frame and reproduces the Vita port's
per-frame right-touchpad gesture.

The same candidate addresses the silent audio path. `AudioTrack` used to call
`SDL_OpenAudioDevice` without ever initializing `SDL_INIT_AUDIO`, then silently
kept device ID zero. It also confused the engine's 1 MiB producer ring with a
hardware period and requested a roughly six-second buffer. Audio now has
explicit initialization, device enumeration/fallback and a 1024-frame period.
The game's 40 obsolete VFP short-vector mixer instructions are expanded into
validated scalar A32 trampolines for the Cortex-A35, which ignores FPSCR
LEN/STRIDE. A qemu register-level self-test proves all 40 expansions reproduce
the original vector operations, while a disassembly audit proves the list
covers all 40 arithmetic opcodes in all 20 LEN regions. The local dummy device
confirms the queue is consumed in real time; audible speaker output awaits the
R36S test.

Current device-test binary:

```text
SHA256 d0ba9983a13a1cf7dbd7a7c5c26d57d544cd9e435ffe80308a03799ea20390de
size   7215740 bytes
```

## ROCKNIX

Set the device's GPU driver to **libmali** (ROCKNIX's own setting; `gpudriver`
reports the current one). The port has been confirmed running that way on an
RG DS.

In **panfrost** mode it now reaches a live GL context — earlier releases could
not, because the port's bundled libraries shadowed what Mesa's driver needed
and glvnd then loaded no driver at all — but it renders black and crashes
shortly after. That is a fixed-function GLES 1.1 path on Mesa/Panfrost, which
is a different problem from this port's; the crash lands on an address that is
really the value of `GL_NEAREST`, so somewhere a call goes through what the
engine stored as an enum. Recorded here in case anyone wants to pick it up.

## Interactive local emulator

`emulator/run.sh` keeps the qemu-arm + Mesa build alive and exposes cursor,
touch, controls, screenshots and logs through a shared control directory.
`emulator/mcp_server.py` publishes the same operations as an MCP server. See
`emulator/README.md`.

This path reproduced the real-device graphics failure locally and then
verified the PVRTC software fallback visually: the same menu now has a fully
textured 3D environment. The immutable M1-M7 harness remains separate and
unchanged.

## Diagnostics

Every device launch writes `ports/masseffect/log.txt`. Important lines:

```text
TRACE: module loaded
TRACE: mounted extracted content at /published
TRACE: VFP short vectors: expanded 40/40 audio instructions
TRACE: AudioTrack: SDL audio ready driver=...
TRACE: AudioTrack: opened device=...
TRACE: AudioTrack: PCM write=...
TRACE: framebuffer non-black
TRACE: summary assets=N textures=N draws=N
FATAL: ...
```

Never redistribute the game `.so` or extracted assets.


## Android APK (OnePlus 13s и другие AArch64-телефоны без AArch32)

Папка `apk/` — обёртка по образцу `../nova3probe`: игра (ARM32) исполняется в собственном qemu-arm 11.1.2
(в APK как `libqemu.so`), все вызовы GLES2 уходят через пайп на GPU телефона (`src/gl_bridge.cpp` на стороне
гостя, `apk/src/com/jamescrumble/meinfiltrator/GlBridge.java` на стороне Android). Сборка и установка:
`tools/build_install.sh` (только `noinstall` — без установки), цикл отладки: `tools/iter.sh`.
Данные игры (APK + cache zip, не в репозитории) кладутся в `Android/data/com.jamescrumble.meinfiltrator/files/`;
первый запуск распаковывает их в приватную папку приложения.

Игра этой сборки — чистый GLES2 (177 импортов `gl*`), а не GLES1 (упоминания GLES1 выше относятся к соседнему порту).

### Как были найдены коды событий указателя (touch)

Игра принимает касания через экспорт `Java_com_ea_blast_TouchSurfaceAndroid_NativeOnPointerEvent(env, obj,
rawEvent, module, id, x, y)`. Значения `rawEvent` НЕ универсальны: их возвращают экспорты
`Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdRawPointerDown/Move/Up`, и в каждой сборке они свои.
Порт с хардкодом `0x6000c/0x4000c/0x8000c` (значения другой сборки) молча игнорировался движком:
касания доходили до `NativeOnPointerEvent`, но не распознавались. Как узнать значения для конкретной
`libMassEffect.so`:

```sh
readelf --dyn-syms -W libMassEffect.so | grep NativeGetIdRawPointer      # адреса экспортов
arm-linux-gnueabihf-objdump -d --start-address=0x35ae94 --stop-address=0x35aea0 libMassEffect.so
```

Каждая функция — три инструкции `mov r0,#N; add r0,r0,#14; bx lr`. Для v1.0.58 gamepad (SHA1 ea58b733…):
Down = 0x60000+14 = **0x6000e**, Move = 0x40000+14 = **0x4000e**, Up = 0x80000+14 = **0x8000e**
(`android/input_bridge.cpp`, enum `ID_RAW_POINTER_*`). Для другой сборки пересчитать так же.

Также для телефона нужно сообщить движку, что тачскрин есть: `MASSEFFECT_TOUCH=1` заставляет
`TouchSurfaceAndroid.IsTouchScreenMultiTouch` вернуть true (`jni/classes/ea_sensors.cpp`).

### Отладка с компьютера без касаний по экрану

Строки файла `inject.txt` рядом с логом (`Android/data/<pkg>/files/`) приложение один раз ставит в очередь команд порта
и удаляет файл (`GameProcess.injectLoop`): `touch 1 x y id` / `touch 0 x y id` (down/up, координаты в пикселях рендера),
`button a down|up`, `sleep <мс>`. Скриншот: `adb shell screencap -p`. Экран телефона должен быть включён.

### Текстуры и шрифт на телефоне (что было сломано и почему)

- **Тёмные силуэты вместо текстур в 3D.** `src/symtab_glprobe.cpp` (обёртки загрузки текстур) искал `glGetString`,
  `glTexImage2D` и `glCompressedTexImage2D` через `SDL_GL_GetProcAddress`. В режиме моста SDL-контекста нет, функции
  оказывались NULL, поэтому поддержка DXT/ATC считалась «отсутствующей», а загрузки молча терялись. Решение —
  `#include "gl_bridge.h"` в этом файле: макрос перенаправляет `SDL_GL_GetProcAddress` на мост. После этого DXT1/3/5 уходят
  на Adreno нативно (в логе: `S3TC: native driver support - DXT1 present, DXT3/DXT5 present`). Любой новый файл порта,
  который ищет GL-функции через SDL, должен подключать `gl_bridge.h`.
- **Квадраты вместо кириллицы в подсказках загрузки.** Движок открывает `/system/fonts/DroidSans(Fallback).ttf`, а порт
  подменял его шрифтом игры (`EurostileLTStd.ttf`, без кириллицы). На Android настоящие системные шрифты есть,
  поэтому `MASSEFFECT_SYSTEM_FONT=/system/fonts/RobotoStatic-Regular.ttf` (задаёт `GameProcess`) заменяет подмену (`fix_path`, правило 0).
- Диагностика: в логе гость один раз пишет формат каждой загрузки текстуры (`gl bridge: texture upload ...`).

### Кнопки «Да/Нет» без текста и `STRING_TAPTOAIM_WARNING`

- В `strings/ENG_US/masseffect.bin` русский патч лежит поверх английского и оставляет ключи `NO` и `YES` без значений
  (индекс значения = 0, пустая строка): диалоги подтверждения показывают пустые кнопки. `apk/.../StringsFix.java`
  при каждом старте (идемпотентно) дописывает в таблицу «Нет» и «Да». Формат SBIN v3: чанк `DATA` (записи пар из u16: id, значение, …),
  `CHDR` — массив элементов (offset, length), `CDAT` — NUL-строки, смещения от `CDAT+12`; для правки добавляются 2 элемента в `CHDR`,
  2 строки в конец `CDAT`, обновляются размеры чанков и индекс значения у записей NO/YES.
  **Важно:** у каждого чанка в заголовке (`tag, size, hash`) хранится хэш FNV-1 (basis 0x811c9dc5, prime 0x01000193) от его содержимого;
  при несовпадении движок отвергает всю таблицу и все строки превращаются в `XXXXX[KEY]XXXXX`. Поэтому любая правка
  `masseffect.bin` обязана пересчитать хэши `DATA`, `CHDR`, `CDAT` (`StringsFix.rehash`). Первые две попытки без пересчёта хэшей
  сломали все строки (в том числе правка всего в 3 байта). Тот же алгоритм на Python: `scratch/fixstr.py`
  (результат Java и Python сверен побайтно).
- Надпись `XXXXX[STRING_TAPTOAIM_WARNING]XXXXX` — это не ошибка порта: ключа нет ни в одной таблице строк игры, он есть только в коде
  (движок так показывает любую отсутствующую строку). Диалог появляется при переключении режима «Tap to Aim» в настройках.

### Меню настроек, звук и NEON-математика (2026-10-11)

Иконка открывает `SettingsActivity`; «Играть» сохраняет `settings.txt` (рядом с логом, `KEY=VALUE`) и запускает игру заново.
Ключи `MASSEFFECT_*` уходят порту как переменные окружения, остальные — переключатели обёртки (`RENDER_HEIGHT`, `FPS_COUNTER`,
`AUDIO_LATENCY_MS`, `CPU_PRIME`, `PROFILING`, `TRACE`); всё применяется в `GameProcess.applySettings` после `engine_env.txt`.

**Звук.** SDL-драйвер `disk` сам себя тактирует сном, который не равен периоду, поэтому шёл чуть быстрее реального времени, и
запас копился в пайпе и буфере `AudioTrack` (звук отставал до ~0,75 с). Теперь темп задаёт динамик: сон короче периода
(`SDL_DISKAUDIODELAY=15`), пайп ужат до одного периода (4096 байт), буфер трека минимальный. Поток SDL блокируется на пайпе.

**NEON-матрицы** (`src/neon_math_patch.cpp`). Четыре умножения `Vectormath::Aos::Matrix4` — верх профиля игры — заменены
NEON-телами той же арифметики (тот же порядок VMUL/VMLA, результат бит-в-бит кроме денормалей и NaN). Выигрыш зависит от
процессора под игрой (в нашем qemu переход VFP<->NEON стоит записи FPCR), поэтому при старте порт замеряет оригинал и NEON
и ставит только то, что быстрее с запасом 15%; результат — строки `neon math:` в логе. `MASSEFFECT_NEON_MATH=0` выключает,
`=1` ставит без замера.
