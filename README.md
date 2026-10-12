# Mass Effect Infiltrator Bridge

**English:** run **Mass Effect Infiltrator 1.0.58** (EA / IronMonkey Studios, 32-bit ARM) on 64-bit-only Android
phones that have no AArch32 support. The game runs under a patched `qemu-user`, and its OpenGL ES calls are
forwarded to the phone's real GPU. Up to 120 fps on a OnePlus 13s (Snapdragon 8 Elite). No game files are
included: you need your own copy. Ready-made APK:
[Releases](https://github.com/JamesCrumble/mass-effect-infiltrator-bridge/releases).
The rest of this page is in Russian.

Запуск 32-битной **Mass Effect Infiltrator** (EA / IronMonkey Studios, v1.0.58, только `armeabi`) на 64-битных
Android-телефонах, в которых нет поддержки AArch32. Проверено на OnePlus 13s (Snapdragon 8 Elite, Android 15).

Игра исполняется в собственном пропатченном `qemu-arm` (linux-user), а её вызовы OpenGL ES пробрасываются
на настоящий GPU телефона. Звук, касания, пауза и возврат в приложение работают.

**Игра не распространяется.** Нужны ваши собственные APK версии 1.0.58 и cache-архив с данными игры.

[Состояние](#состояние) · [Установка](#установка) · [Настройки](#настройки) · [Как это устроено](#как-это-устроено) ·
[Производительность](#производительность) · [Структура](#структура) · [Сборка](#сборка) · [Диагностика](#диагностика) ·
[Другие игры](#другие-игры) · [Лицензия и авторство](#лицензия-и-авторство)

## Состояние

- Игра проходится: меню, миссии, бой, контрольные точки.
- Скорость на OnePlus 13s при рендере 720p и лимите 120: меню 117–120 fps, в бою 98–120 (медиана 116).
- Лимиты 30 и 90 и поведение игры на высокой частоте кадров (скорость анимации и физики) подробно не проверялись.
- Проверено на одном устройстве. На других чипах возможны отличия в драйвере GLES.

Скорость ограничивает не GPU, а процессор: игра упирается в один поток, а Android держит частоту ядер ниже
максимума (см. [Производительность](#производительность)).

## Установка

Готовая сборка лежит на странице [Releases](https://github.com/JamesCrumble/mass-effect-infiltrator-bridge/releases):
`MassEffect-Bridge-<версия>.apk` и `INSTALL.txt`.

1. Установите APK (пакет `com.jamescrumble.meinfiltrator`).
2. Положите в `/sdcard/Android/data/com.jamescrumble.meinfiltrator/files/` два файла: оригинальный APK игры 1.0.58
   и cache-архив `.zip` с её данными. Имена не важны, важны расширения.
3. Откройте приложение, при необходимости поменяйте настройки и нажмите «Играть».

Первый запуск дольше обычного: распаковываются sysroot (~100 МБ) и данные игры. Если что-то не так, причина
пишется на экране и в `meinfiltrator.log` в той же папке.

**Обновление со старого имени пакета.** В первых тестовых сборках пакет назывался иначе, и для Android это другое
приложение: поверх оно не обновится. Прогресс переносится так: сборка `APP_ID=<старый пакет> tools/build_install.sh noinstall`
обновляет старое приложение и на его экране настроек выгружает сохранения в папку `migrate` рядом с файлами игры;
эту папку нужно переложить в `Android/data/com.jamescrumble.meinfiltrator/files/`, новое приложение заберёт её при запуске игры.

## Настройки

Экран настроек открывается при старте приложения. Значения хранятся в `settings.txt` рядом с файлами игры.

| Настройка | Ключ | Что делает |
|---|---|---|
| Разрешение рендера (высота) | `RENDER_HEIGHT` | 480 / 540 / 720, по умолчанию 720; ниже — быстрее и крупнее интерфейс |
| Лимит кадров | `FPS_LIMIT` | 30 / 60 / 90 / 120, по умолчанию 60; та же частота запрашивается у экрана и задаётся ограничителю самой игры |
| Счётчик FPS | `FPS_COUNTER` | Показывает fps поверх игры |
| Буфер звука | `AUDIO_LATENCY_MS` | Размер буфера AudioTrack |
| Плотность экрана для движка (DPI) | `MASSEFFECT_SCREEN_DPI` | Влияет на чувствительность управления; эксперимент |
| Быстрая математика матриц (NEON) | `MASSEFFECT_NEON_MATH` | Авто / выкл / вкл: замена самых горячих функций игры |
| Кадров «в полёте» | `MASSEFFECT_GL_INFLIGHT` | Сколько кадров мост может держать в очереди к GPU, по умолчанию 2 |
| Только быстрые ядра (6–7) | `CPU_PRIME` | Привязывает qemu к двум самым быстрым ядрам; по умолчанию включено |
| Профилирование в логе, TRACE | `PROFILING`, `TRACE` | Диагностика, см. ниже |

Рядом с логом можно положить `resolution.txt` (например `1056x480`) и `cpu_mask.txt`: они важнее значений из меню.

## Как это устроено

```
 APK-обёртка (Java, AArch64)                    дочерний процесс
 ┌──────────────────────────────┐   pipe   ┌─────────────────────────────────────────┐
 │ SettingsActivity             │ ◄──────► │ qemu-arm (наш патч, статический AArch64) │
 │ MainActivity + SurfaceView   │  команды │  └ masseffect (порт-лоадер, ARM32, glibc)│
 │ GlBridge: исполняет GLES     │   GLES   │      ├ bionic ELF loader                 │
 │ AudioTrack, касания          │          │      ├ libMassEffect.so (игра)           │
 │ PerfHint (ADPF, телеметрия)  │          │      ├ JNI и Android API на заглушках    │
 └──────────────────────────────┘          │      └ GL-мост: сериализует вызовы GLES  │
                                           └─────────────────────────────────────────┘
```

- **Порт-лоадер** (`src/`, `loader/`, `thunks/`, `jni/`, `android/`) загружает библиотеку игры, собранную под bionic,
  в процесс с glibc и подменяет libc, JNI и системные вызовы Android. Это порт Mass Effect Infiltrator для
  PortMaster; его собственное описание — в [docs/PORT.md](docs/PORT.md).
- **GL-мост** (`src/gl_bridge.cpp` ↔ `apk/src/.../GlBridge.java`): вызовы GLES 2.0 пишутся в pipe и исполняются
  на стороне Java в контексте GLES 3 на SurfaceView.
- **Звук**: вывод порта → FIFO → AudioTrack, темп задаёт динамик телефона.
- **qemu** (`qemu/`): QEMU 11.1.2 с нашим патчем, см. ниже.

Подробный журнал разработки, выводы и тупиковые ветки — в [PROJECT_STATE.md](PROJECT_STATE.md).

## Производительность

Штатный qemu исполняет игру слишком медленно, поэтому используется QEMU 11.1.2 с набором правок под
AArch64-хост (подробно в [qemu/README.md](qemu/README.md)):

- **Плавающая точка на процессоре телефона.** Скалярные VFP-операции add/sub/mul/div вставляются прямо
  в сгенерированный код одной командой, без вызова функции. Остальные операции VFP и преобразования float↔int
  считаются одной командой процессора внутри хелпера вместо softfloat.
- **NEON f32 целым регистром.** Сложение, вычитание, умножение, умножение с накоплением, сравнения и min/max
  считаются сразу для всех четырёх элементов на NEON телефона, без переключения режима FPCR.
- **Флаги и сравнения в коде.** Сравнения VCMP и запись флагов NZCV(Q) делаются в сгенерированном коде.
- **Косвенные переходы.** Быстрый поиск блока для возвратов и вызовов по указателю: флаги собираются
  на месте, кэш переходов увеличен с 4096 до 65536 записей и хранит всё нужное для попадания.
- **Сцепление блоков** напрямую через границу страницы.
- Обходы ограничений Android: системные вызовы, которые фильтр seccomp приложений запрещает, не используются.

Точность плавающей точки проверяется тестами `qemu/fptest.c`, `cmptest.c`, `neontest.c`, `neoncmptest.c`:
результат должен совпадать с `QEMU_STRICT_FP=1` (чистый softfloat).

Особенности этой игры:

- У движка свой ограничитель кадров, зашитый на 60. Порт при загрузке заменяет это число значением лимита кадров
  (`patch_fps_limit` в `src/patch.cpp`); без этого игра не поднималась выше ~61 fps при любой настройке.
- На быстрых ядрах игра держит ~117 fps даже при заниженном потолке частоты, на средних — 75–80, поэтому привязка
  к быстрым ядрам включена по умолчанию.

Главный ограничитель на OnePlus 13s — политика энергосбережения системы: она опускает потолок частоты ядер
при холодном телефоне, и fps идёт за этим потолком. Из приложения снять это ограничение не удалось.

## Структура

| Папка | Что |
|---|---|
| `src/`, `loader/`, `thunks/`, `jni/`, `android/` | порт Mass Effect Infiltrator для PortMaster: загрузчик bionic-ELF, JNI, переходники libc, патчи; в `src/` также GL-мост |
| `ports/` | упаковка порта для PortMaster и его описание |
| `apk/` | APK-обёртка: Java-код, манифест, сборка APK и sysroot |
| `qemu/` | патчи qemu 11.1.2, Dockerfile для кросс-сборки, тесты точности, бенчмарк |
| `tools/` | сборка и установка, логи, профилирование |
| `docs/`, `analysis/`, `emulator/`, `harness/` | описание порта, проверочные программы и стенд для запуска на ПК |

В репозитории нет файлов игры, тулчейнов и сборочных каталогов (см. `.gitignore`).

## Сборка

Сборка идёт в WSL (Ubuntu).

Нужны, кладутся в корень проекта и в git не входят:

- `jdk/` — JDK 17;
- `android-sdk/` — build-tools 34.0.0, platform 34, platform-tools;
- `sdl-arm/` — SDL2 под armhf (исходники в `third_party/SDL2-2.0.22.tar.gz`);
- системный armhf-тулчейн (`gcc-arm-linux-gnueabihf`, `g++-arm-linux-gnueabihf`) и Docker для qemu;
- `gamebin/` — ваши файлы игры: APK версии 1.0.58 и cache-архив `.zip`; оба затем кладутся на телефон
  (см. [Установка](#установка));
- `medata/` — только для запуска на ПК: те же файлы в распакованном виде.

```sh
source env.sh
./build.sh                         # порт: build/masseffect (ARM32)
tools/build_install.sh             # порт + APK + установка на телефон по adb
tools/build_install.sh noinstall   # то же без установки
tools/make_apk_release.sh          # релизный APK, архив исходников и INSTALL.txt в release/<версия>/
```

Телефон из WSL виден через виндовый `adb.exe`: используйте обёртку `tools/adb`.
`tools/make_release.sh` и `package_portmaster.sh` собирают пакет порта для PortMaster, а не APK.

### qemu

Исходники QEMU 11.1.2 распаковываются в `qemu-src/qemu-11.1.2`, на них по порядку накладываются
`qemu/patches/nova3.patch`, `neon_nofpcr.patch` и `fast_fp_inline.patch`.

```sh
docker build -t masseffect-qemu-build qemu
docker run --rm -u 1000:1000 -e HOME=/tmp -v "$PWD":/work masseffect-qemu-build \
  bash /work/qemu/build_variant.sh F \
  '-DNOVA3_JMP_BITS=16 -DNOVA3_FAST_LOOKUP -DNOVA3_NEON_NOFPCR -O3 -march=armv8.2-a+lse+rcpc+dotprod -mtune=neoverse-v1'
```

Результат — `qemu/out/qemu-arm-F.stripped`, его забирает сборка APK; `ME_QEMU_VARIANT=E` собирает APK
с прежним вариантом. Не добавляйте SVE в `-march`: у ядер Oryon его нет. Имена файлов патчей и макросов `NOVA3_*`
исторические, менять их не нужно. Наложение трёх патчей на чистые исходники с нуля не перепроверялось:
вариант F собран в дереве проекта Shadowgun Bridge и перенесён сюда файлом.

## Диагностика

- Лог игры: `tools/pull_log.sh` → `logs/phone.log`. Строка `[gl] frames` раз в несколько секунд показывает fps,
  число GL-команд, загрузку моста, текущую частоту и потолок частоты ядра.
- Быстрый просмотр fps и поиск по логу: `tools/fps.sh`, `tools/grep_log.sh`; цикл «собрать — поставить — посмотреть» —
  `tools/iter.sh`.
- Профиль кода игры и счётчики системных вызовов пишутся в лог, пока включена настройка «Профилирование в логе».
- Профиль самого qemu: `simpleperf record --app com.jamescrumble.meinfiltrator -p <pid qemu> -e cpu-clock`
  (приложение `profileable`), адреса сопоставляются с символами несжатой сборки qemu.
- Частоты, ядра и температуры: `tools/cpu_diag.sh`.

## Другие игры

Тот же способ запуска — qemu-user и мост GLES на GPU телефона — для других 32-битных игр:

- [Shadowgun Bridge](https://github.com/JamesCrumble/shadowgun-bridge) — SHADOWGUN 1.7.0 (Unity 5.3)
- [NOVA3 Bridge](https://github.com/JamesCrumble/NOVA3-Bridge) — N.O.V.A. 3 1.0.7

## Лицензия и авторство

GPL-3.0, см. [LICENSE](LICENSE). Происхождение заимствованных частей и их лицензии — в [NOTICE.md](NOTICE.md).
Патч qemu — GPL-2.0-or-later, как сам QEMU.

Порт Mass Effect Infiltrator для PortMaster, на котором основан проект (`src/`, `loader/`, `thunks/`, `jni/`,
`android/`, `ports/`), создан EapRules. Android-мост — APK-обёртка `apk/`, проброс GLES на GPU телефона
(`src/gl_bridge*`), патчи qemu (`qemu/`) и скрипты для телефона в `tools/` — написан JamesCrumble.

Mass Effect Infiltrator © Electronic Arts. Проект не связан с правообладателем и не содержит файлов игры.
