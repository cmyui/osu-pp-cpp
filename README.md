# osu-pp-cpp

osu!'s star rating calculation for osu!standard, ported to C++. It gives
the same values as osu!'s own calculator, bit for bit: every attribute and
every per-object skill difficulty, checked against osu! on hundreds of maps
and mod combinations.

It's the calculation in [ppy/osu](https://github.com/ppy/osu)'s
`osu.Game.Rulesets.Osu/Difficulty` (calculator version 20260706), at
**2026.1026.0**: the version osu!'s score processing runs
(`ppy/osu-queue-score-statistics`). Its difficulty code is the same as in
2026.730.0, which `ppy/osu-difficulty-calculator` runs for the star ratings
osu! shows.

## What it calculates

- **Difficulty attributes** (osu!'s `OsuDifficultyAttributes`): the star
  rating, and the aim, speed, reading and, with Flashlight, flashlight
  ratings, with the counts and factors osu!'s performance calculation uses.
- **Each skill's difficulty at each object**, in time order (osu!'s
  `Skill.GetObjectDifficulties`).
- **Timed attributes**: the attributes counting the map only up to each
  object (osu!'s `CalculateTimed`), for ratings as a map is played.

- **osu!'s rating formulas**, for working with the parts: a skill's rating
  from its difficulty value (`aim_rating`, `rating`), and the star rating
  from the skills' ratings (`star_rating`).

It doesn't calculate performance points (pp), legacy score or max combo.

## Using it

It's one header, `include/osu_pp/difficulty.h`, needing C++20. With CMake:

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
   and osu!'s attributes, per-object difficulties and timed attributes.
3. `tests/compare` rates the same objects with the library and checks every
   value is equal.

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
