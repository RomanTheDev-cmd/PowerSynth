# PowerSynth

Полифонический синтезатор MyDarknessDays для лидов, пэдов, атмосфер, стэбов и арпеджио в Techno, Schranz, Hard Techno, Rave и Acid. Мощные текстуры и мрачные краски; без сэмплов. Третий плагин серии после PowerKick и PowerBass: тот же движок проверки, тот же интерфейс, своя чёрно-зелёная палитра.

Форматы:

- **macOS:** AU (для Logic) и VST3, универсальная сборка Apple Silicon + Intel, отдельное приложение Standalone. macOS 10.13 и новее на Intel, macOS 11 и новее на Apple Silicon.
- **Windows 10/11 x64:** VST3 и отдельное приложение.

## Установка

**Установщики** из папки `Установщики`: `PowerSynth-1.0.0-macOS.pkg` и `PowerSynth-1.0.0-Windows-x64-Setup.exe`.

**macOS вручную** из `Сборки/macOS`: `PowerSynth.component` → `~/Library/Audio/Plug-Ins/Components/`, `PowerSynth.vst3` → `~/Library/Audio/Plug-Ins/VST3/`. На этом Mac плагин уже установлен. В Logic: AU Instruments → MyDarknessDays → PowerSynth (после перезапуска Logic).

**Windows вручную:** папку `PowerSynth.vst3` из `Сборки/PowerSynth-1.0.0-Windows-x64.zip` целиком в `C:\Program Files\Common Files\VST3\`.

## Регуляторы

Сверху — крупная форма волны текущего звука (щелчок проигрывает ноту C3) и **MASTER** с индикатором. Ниже два ряда: **OSC A → OSC B / NOISE → FILTER** (путь звука) и **AMP**, **LFO**, **FX**. В каждом блоке крупная главная ручка с зелёной дугой.

| Блок | Регуляторы | Что делает |
|---|---|---|
| OSC A | **SHAPE**, UNISON, DETUNE, OCT, LEVEL | SHAPE плавно переходит sine → triangle → saw → square (полосно-ограниченные). UNISON — до 7 копий, разведённых по панораме; DETUNE — их расстройка до ±50 центов |
| OSC B / NOISE | **LEVEL**, SHAPE, OCT, SEMI, FINE, FM, NOISE | Второй генератор с октавой, полутонами и расстройкой. FM — фазовая модуляция OSC A генератором B (металл, колокола, скрежет). NOISE — белый шум |
| FILTER | LP24 / LP12 / BP / HP, **CUTOFF**, RESONANCE, ENV, F DECAY | Фильтр с отслеживанием высоты ноты; ENV — глубина огибающей фильтра в обе стороны |
| AMP | ATTACK, DECAY, SUSTAIN, RELEASE, GLIDE, POLY / MONO / LEGATO | POLY — 8 голосов; при нехватке самый тихий или отпущенный голос гаснет за 3 мс (без щелчка). MONO — каждая нота с новой атакой; LEGATO — связные ноты без новой атаки; GLIDE в обоих |
| LFO | SIN / TRI / SAW / SQR / RND, цель PITCH / CUTOFF / SHAPE / AMP, SYNC, **AMOUNT**, RATE, DIV | SYNC привязывает скорость к темпу проекта (DIV от 1/1 до 1/32, триоли, 1/8 с точкой) |
| FX | SAT / CLIP / FOLD / CRUSH, **DRIVE**, CHORUS, DELAY, REVERB, WIDTH | Перегруз с передискретизацией, хорус, пинг-понг дилей в темпе проекта, реверберация. WIDTH расширяет только то, что выше 150 Гц |

**DIGITAL | ANALOGUE** в шапке — как в PowerKick и PowerBass. DIGITAL — точный звук (по умолчанию). ANALOGUE — дрейф строя, у каждой ноты и копии унисона своя небольшая расстройка и срез фильтра, тёплая чётная гармоника, мягче верх при той же громкости. Переключение плавное, режим сохраняется при смене пресета.

**ADVANCED:** передискретизация перегруза 2x / 4x / 8x (пресеты её не меняют), атака, сустейн и релиз огибающей фильтра, чувствительность к velocity, время и обратная связь дилея, размер реверберации, диапазон pitch bend.

**Пресеты:** щелчок по имени открывает список: слева категории (Leads, Rave & Hoover, Acid, Pads, Atmospheres, Stabs, Arps & Plucks) и свои пресеты, сверху поиск и типы звука (Saw, Square, FM, Noise). Свои пресеты — `Документы/PowerSynth/Presets`, формат `.pspreset`. 118 заводских пресетов: 20 сделаны вручную, 98 подобраны по признакам референсов TSOHT (спектр относительно ноты, шумность, плотность, атака, длительность звучания, движение, стерео). Имя говорит, на какой материал похож звук: Industrial Lead, Schranz Hook, Trance Melodie, Ravy Saw, Metal Rave, Dark Acid, Theme Pad, Abstract Atmo, Machine Texture, Schranz Shot, Screech, Horror Bell, Driving Sequence, Theme Arp и другие с номерами. Все выровнены по громкости: true peak −1 dBTP на стандартной фразе (с межотсчётными пиками, как у PowerKick).

## Сборка и проверки

Сборка как у PowerBass (JUCE 8, CMake). Проверки:

- `powersynth_tests` — движок: все пресеты (конечность, пик, норма громкости, отсутствие постоянной составляющей, затухание), полифония и отбор голосов без щелчков, MONO и LEGATO, независимость от размера блока и частоты дискретизации, строй, pitch bend, LFO в темпе, ANALOGUE, нагрузка на процессор. `--calibrate` — расчёт MASTER для нормы громкости (true peak −1 dBTP, два прохода).
- `powersynth_wrapper_tests` — состояние, пресеты, моно-выход, окно на трёх размерах без наложений. `--snapshot <папка>` — снимки окна.
- Подбор пресетов: `Tools/ref_synth_features.py` (признаки референсов → `Референсы/synth_features.pkl`, только числа), `Tools/fit_synth.py fit|apply|audit` (подбор к медоидам каждой категории, запись в `FactoryPresets.inc` между метками), затем `--calibrate` для MASTER.
- Выпуск: `bash ../PowerBass/Tools/release_plugin.sh PowerSynth "<метка>"` — Mac и Windows, тесты на arm64, x86_64 и Wine, подпись, установка, auval, pluginval.
