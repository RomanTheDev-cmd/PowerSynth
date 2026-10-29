# PowerSynth

Полифонический синтезатор MyDarknessDays для лидов, пэдов, атмосфер, стэбов и арпеджио в Techno, Schranz, Hard Techno, Rave и Acid. Мощные текстуры и мрачные краски; без сэмплов. Третий плагин серии после PowerKick и PowerBass: тот же движок проверки, тот же интерфейс, своя чёрно-зелёная палитра.

Форматы:

- **macOS:** AU (для Logic) и VST3, универсальная сборка Apple Silicon + Intel, отдельное приложение Standalone. macOS 10.13 и новее на Intel, macOS 11 и новее на Apple Silicon.
- **Windows 10/11 x64:** VST3 и отдельное приложение.

## Движок 2.0 (2026-10-29)

Отзыв Романа о 1.0: «никакущий… просто классический синтезатор… агрессии нет вообще и какого-то характера… обычные звуки аля муг вместо современных шранц и техно вещей». Движок 1.0 был классическим вычитающим (morph sine→square, чистый фильтр, мягкий перегруз на выходе). В 2.0 добавлено то, на чём строят современный hard techno и шранц:
скрытые таблицы волн (9 форм), FM на них, деформация WARP (синк, изгиб, ШИМ-хувер, складывание), кольцевая модуляция, питч-огибающая, перегруз внутри фильтра (GRIT), гребёнка COMB, сдвиг частоты перед дисторшном (SHIFT) и OTT-подобное сжатие (SMASH). Все новые ручки по умолчанию выключены — проекты 1.x звучат как раньше.

**Пресеты 2.0:** 70 новых (Leads: Screamer, Hard Supersaw, Industrial Lead, Screech Lead; Rave & Hoover: Hoover Dive, Rave Alarm, Metal Rave Line; Acid: Acid Scream; Pads: Grit Pad, Vox Pad, Industrial Pad; Atmospheres: Machine Air, Abyss Drone, Rhythmic Noise; Stabs: Schranz Shot, Rave Chord, Metal Shot, Screech Hit; Arps & Plucks: Hard Pluck, FM Metal Pluck, Table Arp — у каждого 3–5 вариантов). Библиотека 1.x (118) — категория **Classic**. Все выровнены по true peak −1 dBTP.

## Установка

**Установщики** из папки `Установщики`: `PowerSynth-2.0.0-macOS.pkg` и `PowerSynth-2.0.0-Windows-x64-Setup.exe`.

**macOS вручную** из `Сборки/macOS`: `PowerSynth.component` → `~/Library/Audio/Plug-Ins/Components/`, `PowerSynth.vst3` → `~/Library/Audio/Plug-Ins/VST3/`. На этом Mac плагин уже установлен. В Logic: AU Instruments → MyDarknessDays → PowerSynth (после перезапуска Logic).

**Windows вручную:** папку `PowerSynth.vst3` из `Сборки/PowerSynth-2.0.0-Windows-x64.zip` целиком в `C:\Program Files\Common Files\VST3\`.

## Регуляторы

Сверху — крупная форма волны текущего звука (щелчок проигрывает ноту C3) и **MASTER** с индикатором. Ниже два ряда: **OSC A → OSC B / NOISE → FILTER** (путь звука) и **AMP**, **LFO**, **FX**. В каждом блоке крупная главная ручка с зелёной дугой.

| Блок | Регуляторы | Что делает |
|---|---|---|
| OSC A | SYNC / BEND / PWM / FOLD, **SHAPE**, UNISON, DETUNE, WARP, OCT, LEVEL, RING | SHAPE: SINE → TRI → SAW → SQUARE, дальше скрытые таблицы HOLLOW, VOX A, VOX O, GROWL, SCREECH, SYNC, BITTER, METAL, INDUST (плавный переход, без наложения частот на любой высоте). WARP деформирует волну: SYNC — крик жёсткого синка, BEND — медь изогнутой фазы, PWM — хувер, FOLD — металл складывания; LFO на SHAPE двигает и WARP. RING — кольцевая модуляция на OSC B. UNISON до 7 копий по стерео |
| OSC B / NOISE | **LEVEL**, SHAPE, OCT, SEMI, FM, NOISE | Второй генератор с теми же формами и таблицами; FM — фазовая модуляция OSC A генератором B (с таблицами — современный металл); SEMI задаёт соотношение для FM и RING; FINE — в ADVANCED |
| FILTER | LP24 / LP12 / BP / HP / COMB, **CUTOFF**, RESONANCE, ENV, F DECAY, GRIT | GRIT — перегруз на входе фильтра и между его ступенями: резонанс рычит (acid, screech). COMB — гребёнка по высоте ноты: металл и индустриальный звон (RESONANCE — длина звона, CUTOFF — яркость) |
| AMP | POLY / MONO / LEGATO, ATTACK, DECAY, SUSTAIN, RELEASE, GLIDE, PITCH, P.TIME | POLY — 8 голосов, отобранный гаснет за 3 мс. PITCH — сдвиг высоты в начале ноты (+ удар сверху для стэбов и шотов, − подъём снизу для хувера), P.TIME — за сколько он уходит |
| LFO | SIN / TRI / SAW / SQR / RND, цель PITCH / CUTOFF / SHAPE / AMP, SYNC, **AMOUNT**, RATE, DIV | SYNC привязывает скорость к темпу проекта (DIV от 1/1 до 1/32, триоли, 1/8 с точкой) |
| FX | SAT / CLIP / FOLD / CRUSH, **DRIVE**, CHORUS, DELAY, REVERB, WIDTH, SMASH, SHIFT | SHIFT — небольшой сдвиг частоты перед дисторшном (звук машины). SMASH — трёхполосное сжатие вверх и вниз в духе OTT: громкое прижимается, хвосты поднимаются — современная плотность. Затем хорус, пинг-понг дилей в темпе, реверберация |

**DIGITAL | ANALOGUE** в шапке — как в PowerKick и PowerBass. DIGITAL — точный звук (по умолчанию). ANALOGUE — дрейф строя, у каждой ноты и копии унисона своя небольшая расстройка и срез фильтра, тёплая чётная гармоника, мягче верх при той же громкости. Переключение плавное, режим сохраняется при смене пресета.

**ADVANCED:** передискретизация перегруза 2x / 4x / 8x (пресеты её не меняют), тонкая подстройка OSC B (B FINE), атака, сустейн и релиз огибающей фильтра, чувствительность к velocity, время и обратная связь дилея, размер реверберации, диапазон pitch bend.

**Пресеты:** щелчок по имени открывает список: слева категории (Leads, Rave & Hoover, Acid, Pads, Atmospheres, Stabs, Arps & Plucks) и свои пресеты, сверху поиск и типы звука (Saw, Square, FM, Noise). Свои пресеты — `Документы/PowerSynth/Presets`, формат `.pspreset`. 118 заводских пресетов: 20 сделаны вручную, 98 подобраны по признакам референсов TSOHT (спектр относительно ноты, шумность, плотность, атака, длительность звучания, движение, стерео). Имя говорит, на какой материал похож звук: Industrial Lead, Schranz Hook, Trance Melodie, Ravy Saw, Metal Rave, Dark Acid, Theme Pad, Abstract Atmo, Machine Texture, Schranz Shot, Screech, Horror Bell, Driving Sequence, Theme Arp и другие с номерами. Все выровнены по громкости: true peak −1 dBTP на стандартной фразе (с межотсчётными пиками, как у PowerKick).

## Сборка и проверки

Сборка как у PowerBass (JUCE 8, CMake). Проверки:

- `powersynth_tests` — движок: все пресеты (конечность, пик, норма громкости, отсутствие постоянной составляющей, затухание), полифония и отбор голосов без щелчков, MONO и LEGATO, независимость от размера блока и частоты дискретизации, строй, pitch bend, LFO в темпе, ANALOGUE, нагрузка на процессор. `--calibrate` — расчёт MASTER для нормы громкости (true peak −1 dBTP, два прохода).
- `powersynth_wrapper_tests` — состояние, пресеты, моно-выход, окно на трёх размерах без наложений. `--snapshot <папка>` — снимки окна.
- Подбор пресетов: `Tools/ref_synth_features.py` (признаки референсов → `Референсы/synth_features.pkl`, только числа), `Tools/fit_synth.py fit|apply|audit` (подбор к медоидам каждой категории, запись в `FactoryPresets.inc` между метками), затем `--calibrate` для MASTER.
- Выпуск: `bash ../PowerBass/Tools/release_plugin.sh PowerSynth "<метка>"` — Mac и Windows, тесты на arm64, x86_64 и Wine, подпись, установка, auval, pluginval.
