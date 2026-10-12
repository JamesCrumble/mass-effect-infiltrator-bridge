# Mass Effect Infiltrator Bridge (ME Infiltrator -> Android APK для OnePlus 13s): состояние проекта

Цель: повторить для этой игры то, что сделано в `../nova3probe` (APK с qemu-user + мост GLES на GPU телефона).
Образец архитектуры: `../nova3probe/PROJECT_STATE.md` и `../nova3probe/port/android/README.md`.

Обновлено: 2026-10-08.

Рабочая папка: `/home/james/probe/mass-effect-infiltrator-probe` (Windows: `\\wsl.localhost\Ubuntu-20.04\home\james\probe\mass-effect-infiltrator-probe`).
Всё исполняется в WSL (Ubuntu-20.04); VS Code открыт с Windows. Телефон: OnePlus 13s (CPH2723, Snapdragon 8 Elite,
Oryon, только AArch64), подключён по USB, виден через Windows `adb.exe` (обёртка `tools/adb`).

## Правила работы
- По-русски, кратко.
- Команды — только внутри WSL (`wsl -d Ubuntu-20.04 -e ...`), команды Windows не использовать.
- **Установка APK + перезапуск игры — можно всегда. Любая другая команда на телефоне (push, rm, shell, профиль,
  настройки, бенчмарки) — только после явного «да» пользователя.**
- Перед крупными загрузками спрашивать. Ничего не ставить глобально (sudo нет); всё локально в папке проекта
  (`jdk/`, `android-sdk/`, `sdl-arm/`, `qemu/out/`); сборки — в WSL/Docker.
- Скрипты на телефон/через Windows запускать так: `wsl -d Ubuntu-20.04 --cd /home/james/probe -e sh mass-effect-infiltrator-probe/tools/run.sh mass-effect-infiltrator-probe/tools/<x>.sh`.

## Что есть в папке
| путь | что |
|---|---|
| `src/ android/ loader/ jni/ thunks/` | исходный порт EapRules (bionic-лоадер + JNI + libc-thunks), собирается `./build.sh` -> `build/masseffect` (armhf) |
| `gamebin/` | оригиналы пользователя: `Mass-Effect-Infiltrator-v1-0-58-Unlocked.apk`, `00657-...-cache.zip` (не в git) |
| `medata/masseffect/` | распакованная игра: `lib/armeabi/libMassEffect.so` (SHA1 ea58b733… совпал), `assets/` + `assets/published/` (кэш, ~740 МБ) |
| `jdk/ android-sdk/ sdl-arm/` | локальные тулчейны (скопированы из nova3probe) |
| `qemu/out/qemu-arm-D.stripped`, `qemu/patches/nova3.patch` | наш qemu-arm 11.1.2 (вариант D: быстрый float/NEON, lookup_tb_ptr), скопирован из nova3probe |
| `env.sh`, `build.sh`, `tools/` | окружение, сборка, скрипты (adb, pull_log, fps, grep_log, run.sh, cpu_diag, timeline) |

## Сделано (2026-10-08)
- Тулчейны локально; данные игры распакованы в `medata/masseffect` (SHA1 libMassEffect.so совпал); порт собирается `./build.sh`.
- **ИСПРАВЛЕНИЕ**: игра — **чистый GLES2** (177 импортов `gl*`, 64 шейдерных), а не GLES1: описание про GLES1 в README/комментариях
  относится к соседнему порту. Поэтому мост nova3 (GLES2) подошёл почти без переделки.
- Мост перенесён из nova3 portbase: `src/gl_bridge.cpp/.h`, `gl_bridge_gen.inc`, `gl_stats.*`, `port_env.*`, `thunks/khronos/gles1|2.cpp`.
  Добавлены недостающие вызовы: glBindAttribLocation (новый op), glUniform2/3/4 f/i, UniformMatrix2/3, VertexAttrib*, Stencil*Separate,
  Hint, Finish, DetachShader, ValidateProgram, CopyTexImage2D, TexParameter*v, Is*, Get* (заглушки). Генератор: `apk/tools/gen_glbridge.py`.
- `src/main.cpp`: режим моста (`MASSEFFECT_GL_BRIDGE=1`: без SDL-GL, swap -> `gl_bridge_swap`), размер из `MASSEFFECT_WIDTH/HEIGHT`.
- Ввод: команда `touch <1|2|0> x y id` (мультитач в JNI-указатель игры), остальные команды emulator_control как были.
- Обёртка APK (`apk/`, пакет `com.jamescrumble.meinfiltrator`): MainActivity/GameProcess/GlBridge/PerfHint из nova3; без SettingsActivity.
  Первый запуск: из положенных в `Android/data/com.jamescrumble.meinfiltrator/files/` `*.apk` и `*.zip` (cache) извлекает игру в приватную папку.
- Сборка тестового APK: `tools/build_install.sh noinstall` -> `MassEffect-Infiltrator-debug.apk` (56 МБ, подписан debug-ключом `apk/build/debug.keystore`).

## Не проверено / дальше
- Телефон в WSL-adb не виден (`adb devices` пусто) — APK ещё не ставился и не запускался. Нужно подключить телефон (отладка по USB).
- Данные игры на телефон (`gamebin/*.apk` и `*.zip` -> `/sdcard/Android/data/com.jamescrumble.meinfiltrator/files/`) — только после «да».
- Не тестировались: запуск qemu на телефоне, мост (протокол проверялся только сборкой), звук (SDL disk -> FIFO), VFP-патч под наш qemu, масштаб UI на 1544x720.
- Оптимизация (профили, настройки, cpu_mask, lookup_tb_ptr и т.д.) — потом, как в nova3.

## Сессия 2026-10-09
- Тестовый APK установлен и запущен на OnePlus 13s: qemu стартует, мост работает, ~60 FPS, главное меню рисуется.
- Касания не работали: порт слал коды указателя другой сборки. Сборка 1.0.58 gamepad ждёт 0x6000e/0x4000e/0x8000e
  (найдено дизассемблером `NativeGetIdRawPointer*`) — исправлено в `android/input_bridge.cpp`; описано в README (раздел «Android APK»).
  Плюс `MASSEFFECT_TOUCH=1` (движок считает, что тачскрин есть). Сенсор работает (подтверждено пользователем).
- Канал отладки `inject.txt` (см. README) и снятие скриншотов через adb — рабочие.
- Исправлено: текстуры 3D (загрузки терялись из-за `SDL_GL_GetProcAddress` в `symtab_glprobe.cpp`, нужен `gl_bridge.h`) и кириллица
  в подсказках (системный шрифт телефона через `MASSEFFECT_SYSTEM_FONT`). Подробности — README. Миссия рендерится корректно, ~60 FPS.
- Дальше: проверка 20 минут без ошибочного кода выхода (критерий остановки теста от пользователя), потом звук, управление в бою
  (экранные стики/кнопки), оптимизации как в nova3.
- 20 минут: игра работала без падений дольше 20 минут в одном запуске (проверено 00:42 после старта ~00:30 и дальше по скриншотам
  пользователя до ~01:05, `qemu exited` в логе не было); после этого я пересобирал и переустанавливал APK.
- Исправлено: пустые «Да/Нет» (`StringsFix.java`, см. README).
- Открыто: «сверхчувствительное» управление бегом влево/вправо на большом экране. Рабочие гипотезы: пороги движка заданы в пикселях под
  экраны ~480–540 по высоте (Xperia Play 854x480), а рендер сейчас 1544x720; ручки для экспериментов без пересборки (файлы рядом с логом,
  `Android/data/com.jamescrumble.meinfiltrator/files/`): `resolution.txt` (например `1056x480`) и `engine_env.txt` (`MASSEFFECT_SCREEN_DPI=...`).
- Готово и подтверждено пользователем: «Нет»/«Да» в диалогах (`StringsFix.java`, с пересчётом FNV-1 хэшей чанков; формат таблицы строк и
  причина прежних поломок описаны в README). Тест остановлен по просьбе пользователя.
- Осталось: чувствительность бега (ручки `resolution.txt`, `engine_env.txt` с `MASSEFFECT_SCREEN_DPI`), звук, экранные кнопки геймпада,
  оптимизации, реальная проверка `MASSEFFECT_TOUCH` в бою.

## Сессия 2026-10-11
- Пользователь разрешил использовать телефон для тестов (логи, скриншоты, запуск) в этой сессии. Просит объяснять ход работы
  простыми словами.
- **Краш (лог `logs/meinfiltrator_crash.log`, 18-я минута, бой):** SIGSEGV в `m3g::Renderer::RenderRenderNode` (+0x3f0e00,
  `ldr pc,[vtable+0x5c]`): в списке отрисовки вместо `m3g::Node` лежал `m3g::Appearance` (vtable +0xa57ff8, слот +0x5c попал на
  `typeinfo AppearanceBase` +0xa580a0). Причина НЕ установлена. Версии: use-after-free узла или порча списка в `eastl::quick_sort`
  (NaN в `SortedRenderNode::operator<`). Дальше: дамп элементов `SortedRenderNode` (40 байт) в обработчике краша.
- **Меню настроек** (`SettingsActivity`, лаунчер; `settings.txt` рядом с логом; применяются в `GameProcess.applySettings`):
  высота рендера, счётчик FPS, буфер звука, DPI для движка, NEON-математика (авто/выкл/вкл), кадров «в полёте», ядра 6-7,
  профилирование, TRACE. `tools/launch.sh` стартует `MainActivity` напрямую, минуя меню.
- **NEON-матрицы** (`src/neon_math_patch.cpp`): `Matrix4*Vector4`, `transform`, `Matrix4*Matrix4`, `*=` заменены NEON-телами,
  бит-в-бит на обычных числах (тест против оригинальных байт, 400 тыс. раундов). При старте порт сам замеряет оригинал и NEON
  и ставит только выигравшее (строки `neon math:` в логе). На телефоне: matrix*matrix 6,00 -> 2,07 мс на 20 000 вызовов;
  matrix*vector примерно паритет (1,44/1,12 и 1,79/1,83 в разных запусках). `MASSEFFECT_NEON_MATH=0|1` — выкл/принудительно.
- Профиль боя до: `Matrix4*Vector4` 17,3% + `*Matrix4` 4,4% + `transform` 3,6% + `*=` 1,7% (всего ~27% сэмплов, из них ~25% простой).
  После: `*=` 17,7% + `*Matrix4` 1,4% на минутном отрезке (другая сцена, сравнение грубое). Остаток — цена записи FPCR при
  переходах VFP<->NEON в нашем qemu (ленивое FPCR). **Следующий шаг по скорости:** в qemu считать NEON f32 без смены FPCR
  (обычный режим + проверка на денормали/NaN с уходом в медленный путь), пересборка `../nova3probe/tools/qemu_build.sh`.
  И найти, кто зовёт `Matrix4::operator*=` десятки тысяч раз за кадр (`m3g::Transform::PostMultiply`?).
- **Звук отставал:** SDL disk-драйвер шёл чуть быстрее реального времени, запас копился в пайпе (64 КБ) и буфере AudioTrack (64 КБ),
  до ~0,75 с. Теперь часы — AudioTrack: `SDL_DISKAUDIODELAY=15`, пайп 4096 байт (`F_SETPIPE_SZ`), буфер трека минимальный
  (на OnePlus 13s 120 мс), LOW_LATENCY. Строка `[audio]` в логе. На слух пользователем ещё НЕ подтверждено.
- Мелочи: `TZ` задаётся из часового пояса телефона (glibc больше не открывает `/etc/localtime` ~150 раз/с: 1473 -> 7–13 openat за 10 с);
  строка TRACE `StorageDirectory...` печатается 4 раза, а не каждый кадр (было 39 тыс. строк из 47 тыс.).
- Частоты ядер в бою по-прежнему режутся до 1,15–1,5 ГГц (лимитер OxygenOS) — главный тормоз; переключатель «ядра 6-7» не проверялся.
- **Вылет в `RenderRenderNode` — найден дефект движка и исправлен** (`src/render_sort_patch.cpp`): `SortedRenderNode::operator<`
  не транзитивен (шаг «оба с флагом +20 -> по vertex buffer» действует на пару, а не на элемент), `eastl::quick_sort` на таком
  порядке выходит за конец вектора и затаскивает в список устаревшие записи прошлых кадров. Проверено собственным кодом игры
  (копия байт из .so под qemu-arm): 11 из 20 000 сортировок писали за список (до 90 записей), с исправлением 0; пары с равным
  флагом сравниваются как раньше (2 млн пар). Что именно это вызвало тот вылет — вывод по картине краша, прямого доказательства нет;
  проверка — долгая игра без вылета. `MASSEFFECT_NO_SORT_FIX=1` возвращает сравнение движка. Версия с NaN отпала (сравнение целочисленное).
- Правило от пользователя: если сказано «только установи / не запускай» — не запускать игру вообще, даже для проверки; нужен запуск — спросить.
- **qemu вариант E** (в APK по умолчанию, `tools/build_install.sh`; `ME_QEMU_VARIANT=D` возвращает прежний): NEON f32
  (add/sub/mul/mla/mls, в т.ч. по скаляру) считается при обычном FPCR хоста, без переключения в FZ+DN; элементы с денормалями/NaN
  на входе или с результатом в (0, наименьшее нормальное] считаются под разовым переключением FPCR (`neon_f32_std`).
  Флаг сборки `-DNOVA3_NEON_NOFPCR`, правка в `../nova3probe/qemu-src/.../target/arm/tcg/vec_helper.c` (под `#ifdef`, вариант D
  не затронут), патч поверх nova3.patch — `qemu/patches/neon_nofpcr.patch`. Сборка:
  `sh ../nova3probe/tools/qemu_build.sh E "-DNOVA3_JMP_BITS=16 -DNOVA3_FAST_LOOKUP -DNOVA3_NEON_NOFPCR -O3 -march=armv8.2-a+lse+rcpc+dotprod -mtune=neoverse-v1"`,
  затем скопировать `qemu-arm-E.stripped` в `qemu/out/`.
  Точность: `fptest` (хэш 94e3b209 = strict = Debian 7.2) и `qemu/neontest.c` (5 классов, включая NaN/денормали/границу нормальных) —
  совпадает с `QEMU_STRICT_FP=1`. **На телефоне НЕ запускался** (пользователь просил только установить): скорость не измерена,
  смотреть строки `neon math:` в логе после запуска (на варианте D было matrix*matrix 6,00 -> 2,07 мс, matrix*vector ~паритет).
  Попутно: быстрый скалярный путь nova3 (`ff32_*` в softfloat.c) неточен на границе наименьшего нормального при FZ у гостя —
  к игре не относится (VFP игры работает без FZ), но «SOFT»-выражения в vec_helper.c через него не являются эталоном.

## Лимит кадров и ограничитель движка (2026-10-12)
- Настройка «Лимит кадров» (30/60/90/120, `FPS_LIMIT`): частота экрана запрашивается под лимит, цель ADPF = лимит.
- Игра сама держала ~61 fps: `EA::Blast::System::CreateFpsLimiter` создаёт `FpsLimiter` с константой 60
  (`mov r1, #60` по +0x35c1d0), период хранится в целых мс (1000/60 = 16). `patch_fps_limit` в `src/patch.cpp`
  заменяет константу значением `MASSEFFECT_TARGET_FPS` (обёртка передаёт туда `FPS_LIMIT`). Проверено: при 120 меню
  идёт 117-120 fps. В бою и при 30/90 не проверялось; зависит ли скорость игры от частоты кадров - не проверялось.
- qemu вариант F (тот же файл, что в Shadowgun Bridge): запуск проверен, ошибок нет.
- Пакет `com.jamescrumble.meinfiltrator`; перенос сохранений со старого имени - `SaveMigration.java`.
- Замер в бою (1,3-1,9 млн GL-команд/с, лимит 120): «кадров в полёте» 1 + средние ядра - 75-110 fps с просадками до 50-70;
  «кадров в полёте» 2 + `CPU_PRIME=1` (ядра 6-7) - минимум 98, медиана 116. На быстрых ядрах ~117 fps даже при потолке
  1209 МГц, на средних при 1800-2200 МГц - 75-80. Поэтому `CPU_PRIME` теперь включён по умолчанию.
- Профиль главного потока в бою: ~69% busy, хелперы qemu 22% потока, из них `helper_arm_lookup_tb_ptr` 17%; FP почти нет.

