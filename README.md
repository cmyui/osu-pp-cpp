# osu-pp-cpp

osu!'s star rating and performance points (pp) calculations for
osu!standard, ported to C++. They give the same values as osu!'s own, bit
for bit: every attribute, every per-object skill difficulty and every part
of a score's pp, checked against osu! on hundreds of maps and mod
combinations.

They're the calculations in [ppy/osu](https://github.com/ppy/osu)'s
`osu.Game.Rulesets.Osu/Difficulty` (calculator version 20260706), at
**2026.1026.0**: the version osu!'s score processing runs
(`ppy/osu-queue-score-statistics`). Its difficulty code is the same as in
2026.730.0, which `ppy/osu-difficulty-calculator` runs for the star ratings
osu! shows.

## What it calculates

- **Difficulty attributes** (osu!'s `OsuDifficultyAttributes`): the star
  rating, and the aim, speed, reading and, with Flashlight, flashlight
  ratings, with the counts and factors osu!'s performance calculation uses:
  among them the map's max combo, and osu!stable's score multiplier for it.
- **Each skill's difficulty at each object**, in time order (osu!'s
  `Skill.GetObjectDifficulties`).
- **Timed attributes**: the attributes counting the map only up to each
  object (osu!'s `CalculateTimed`), for ratings as a map is played.

- **osu!'s rating formulas**, for working with the parts: a skill's rating
  from its difficulty value (`aim_rating`, `rating`), and the star rating
  from the skills' ratings (`star_rating`).
- **A score's performance points** (osu!'s `OsuPerformanceAttributes`): its
  pp, each part of them (aim, speed, accuracy, flashlight, reading), and
  the misses and slider breaks osu! estimated on the way.

It doesn't score plays: a score's judgements and max combo, and for one set
on osu!stable its total score, are the caller's to give.

## Using it

It's two headers, needing C++20: `include/osu_pp/difficulty.h` for star
rating, and `include/osu_pp/performance.h` for pp, which includes the
first. With CMake:

```cmake
include(FetchContent)
FetchContent_Declare(osu_pp
  GIT_REPOSITORY https://github.com/cmyui/osu-pp-cpp.git
  GIT_TAG <commit>)
FetchContent_MakeAvailable(osu_pp)
target_link_libraries(your_target PRIVATE osu_pp::osu_pp)
```

```cpp
#include <osu_pp/difficulty.h>

osu_pp::Beatmap map = /* your map, as below */;
osu_pp::Mods    mods{.clock_rate = 1.5, .hidden = true};
osu_pp::Difficulty difficulty = osu_pp::calculate(map, mods);
double stars = difficulty.attributes.star_rating;
```

The library reads no files. You give it a map as osu!lazer makes it playable
for a play, from whatever parser you use:

- **Settings:** circle size, approach rate and overall difficulty, with
  Easy, Hard Rock or Difficulty Adjust applied, in single precision as
  osu!lazer keeps them.
- **Objects:** in osu!lazer's order, with Hard Rock's flip applied.
  - Positions are as parsed, before stacking. Give osu!lazer's stack heights
    and the library adds each object's stack offset.
- **Sliders, as well:**
  - their repeat count;
  - their path as osu!lazer computes it: the points relative to the slider,
    and the distance to each, fitted to the slider's length;
  - their nested objects (head, ticks, repeats, tail) with their times and
    positions, in osu!lazer's order.

`Mods` holds what the calculation reads besides that: the clock rate, and
Hidden, Flashlight, Touch Device, Relax and Autopilot, plus osu!lazer's
Magnetised and Deflate.

Parsing the map is the caller's job, so osu!lazer's parsing rules are the
parser's to follow. For example, from 2026.1026.0 osu!lazer clamps object
positions to 0–512, as stable does.

### pp

```cpp
#include <osu_pp/performance.h>

osu_pp::Score score{.accuracy = 0.9871, .max_combo = 1204,
                    .great = 842, .ok = 31, .meh = 2, .miss = 3};
osu_pp::Performance pp =
    osu_pp::performance(difficulty.attributes, map, mods, score);
double total = pp.total;
```

The attributes are `calculate`'s for the same map and mods. pp reads more
of the map and the mods than star rating does:

- **The map:** its HP drain rate with the mods (for Blinds), and its own
  circle size, overall difficulty and HP drain rate before them, with its
  breaks, for osu!stable's score multiplier.
- **The mods:** No Fail, Easy, Half Time, Hard Rock, Double Time, Spun Out,
  Blinds, Traceable and Score V2, and whether sliders are judged as on
  osu!stable (`classic_slider_accuracy`, below).

osu!stable's judgements don't count a score's slider breaks, so osu! reads a
score one of three ways, by what it can know of them:

| The score | `mods.classic_slider_accuracy` | `score.legacy_total_score` | Its slider breaks |
|---|---|---|---|
| set on osu!stable | `true` | its total score | estimated from its total score |
| set on osu!lazer with Classic's "No slider head accuracy" | `true` | none | estimated from its max combo |
| set on osu!lazer | `false` | none | its `slider_tail_hits` and `large_tick_misses` |

`score.accuracy` is osu!'s accuracy for the score: for one set on
osu!stable, osu!stable's, from its 300s, 100s, 50s and misses.

## Why the values match exactly

Each section of the header is one of osu!'s files, in its order. Every
operation is done in the order and precision osu! does it: positions and
their distances in single precision, everything else in double.

Three things outside the code matter:

- **No fused operations.** osu! never fuses floating-point operations it
  doesn't ask to, so code using the library must be built with
  `-ffp-contract=off`. The CMake target sets it.
- **.NET's `double.Lerp`.** osu! uses it in one place, and on hardware with
  fused multiply-add .NET computes it as one. The library uses `std::fma`
  there too, so it matches osu! on such hardware, which is every current
  x86-64 and ARM64 chip.
- **The math library.** Functions like `pow` and `exp` come from the
  platform's math library, as .NET's do, so the values match osu!'s on the
  same platform.

## Testing

`tests/run.sh` checks the library against osu!'s own calculation. It needs
the .NET 10 SDK.

1. It builds `tools/reference`, a small program that loads each map with
   osu!'s code, pinned to the same version.
2. For each set of mods, the reference writes out the map's playable objects
   and osu!'s attributes, per-object difficulties and timed attributes, and
   osu!'s pp for made-up scores on it: full combos and broken ones, with
   100s, 50s and misses, slider tails dropped and ticks missed, each with
   no osu!stable total score and with totals from small to huge, and some
   without Classic's slider accuracy.
3. `tests/compare` rates the same objects with the library, and the same
   scores, and checks every value is equal.

By default it runs on `tests/maps`: made-up maps, written by
`tests/maps/generate.py`, that cover jumps, streams, rhythm changes, stacks,
every slider curve, repeats, spinners and a break. Give it your own maps or
folders to check more:

```sh
tests/run.sh                 # the test maps
tests/run.sh ~/osu/Songs     # every .osu under a folder
```

## Licence

MIT, as osu! is: see `LICENSE`.
