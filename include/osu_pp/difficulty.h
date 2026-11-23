// osu-pp-cpp: osu!'s star rating calculation for osu!standard, ported to C++
// from ppy/osu at 2026.1026.0 (see README.md). Each section below is one of
// osu!'s source files, named in its heading, kept in its order and with its
// arithmetic: positions in single precision, as osu! keeps them, and every
// operation in the order and precision osu! does it, so the results match
// osu!'s bit for bit on the same platform.
//
// It reads no files: the caller gives it a map's objects as osu!lazer
// prepares them (Beatmap), from whatever parser it uses.
//
// Licensed under the MIT licence (LICENSE), as osu! is.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <numbers>
#include <optional>
#include <utility>
#include <vector>

namespace osu_pp {

// ------------------------------------------------------------------ input

// osuTK's Vector2: osu! keeps positions in single precision.
struct Vector2 {
  float x = 0, y = 0;

  friend Vector2 operator+(Vector2 a, Vector2 b) { return {a.x + b.x, a.y + b.y}; }
  friend Vector2 operator-(Vector2 a, Vector2 b) { return {a.x - b.x, a.y - b.y}; }
  friend Vector2 operator*(Vector2 a, float s) { return {a.x * s, a.y * s}; }
  // (float)Math.Sqrt(X * X + Y * Y)
  float length() const {
    return static_cast<float>(std::sqrt(static_cast<double>(x * x + y * y)));
  }
  static float dot(Vector2 a, Vector2 b) { return a.x * b.x + a.y * b.y; }
  static float distance(Vector2 a, Vector2 b) {
    return static_cast<float>(std::sqrt(static_cast<double>(
        (b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y))));
  }
};

// A part of a slider osu! checks: its head, a tick, a repeat or its tail,
// where it is before stacking.
struct NestedObject {
  enum Kind : std::uint8_t { head, tick, repeat, tail };
  Kind    kind = head;
  double  time = 0;  // ms
  Vector2 position;
};

// A slider's path as osu!lazer computes it (SliderPath's calculatedPath and
// cumulativeLength): points relative to the slider's position, and the
// distance along the path to each, its length fitted to the slider's.
struct SliderPath {
  std::vector<Vector2> points;
  std::vector<double>  lengths;
};

// A hit object of the playable map. Positions are before stacking: the
// calculation adds each object's stack offset, from its stack height.
struct HitObject {
  enum Kind : std::uint8_t { circle, slider, spinner };
  Kind    kind = circle;
  double  start_time = 0;  // ms
  double  end_time = 0;    // ms; a circle's is its start
  Vector2 position;
  int     stack_height = 0;
  // Sliders only.
  int                       repeat_count = 0;
  SliderPath                path;
  std::vector<NestedObject> nested;  // in osu!lazer's NestedHitObjects order
};

// A break in a map, as its file gives it.
struct Break {
  double start_time = 0;  // ms
  double end_time = 0;
};

// A map as osu!lazer makes it playable with a play's mods: the settings in
// single precision, as osu!lazer keeps them, after Easy, Hard Rock and
// Difficulty Adjust; objects in its order, with Hard Rock's flip applied,
// and with osu!lazer's stack heights.
struct Beatmap {
  float                  circle_size = 5;
  float                  approach_rate = 5;
  float                  overall_difficulty = 5;
  float                  drain_rate = 5;  // read only by Blinds' pp
  std::vector<HitObject> objects;

  // The map's own settings, before mods, and its breaks: osu!stable's
  // score multiplier is the map's own (see legacy_score_base_multiplier).
  float              own_circle_size = 5;
  float              own_overall_difficulty = 5;
  float              own_drain_rate = 5;
  std::vector<Break> breaks;
};

// The mods the calculation reads, besides those already applied to the map.
struct Mods {
  double clock_rate = 1;  // Double Time's, Nightcore's, Half Time's, ...
  bool   hidden = false;
  bool   hidden_only_fades_approach_circles = false;
  bool   flashlight = false;
  bool   touch_device = false;
  bool   relax = false;
  bool   autopilot = false;
  float  magnetised = 0;            // osu!lazer's attraction strength, or 0
  float  deflate_start_scale = 0;   // osu!lazer's Deflate, or 0 without

  // Read only by performance (see performance.h). Classic's slider accuracy
  // is osu!lazer's Classic mod's "No slider head accuracy", as osu!stable
  // judges sliders; osu! gives it to every score set on osu!stable.
  bool no_fail = false;
  bool easy = false;
  bool half_time = false;    // or Daycore
  bool hard_rock = false;
  bool double_time = false;  // or Nightcore
  bool spun_out = false;
  bool blinds = false;
  bool traceable = false;
  bool score_v2 = false;
  bool classic_slider_accuracy = false;
};

// OsuDifficultyAttributes' difficulty values.
struct Attributes {
  double                star_rating = 0;
  double                aim = 0;
  double                aim_difficult_slider_count = 0;
  double                speed = 0;
  double                speed_note_count = 0;
  double                reading = 0;
  std::optional<double> flashlight;  // with Flashlight only
  double                slider_factor = 0;
  double                aim_difficult_strain_count = 0;
  double                speed_difficult_strain_count = 0;
  double                reading_difficult_note_count = 0;
  double                aim_top_weighted_slider_factor = 0;
  double                speed_top_weighted_slider_factor = 0;

  // And those osu!'s performance calculation reads besides: the objects'
  // counts and the map's max combo, and from osu!stable's scoring (see
  // LegacyScoreUtils and OsuLegacyScoreSimulator) the score of an average
  // object's slider ticks and spinner spins, the map's score multiplier,
  // and the combo bonus a full combo makes.
  int    max_combo = 0;
  int    hit_circle_count = 0;
  int    slider_count = 0;
  int    spinner_count = 0;
  double nested_score_per_object = 0;
  double legacy_score_base_multiplier = 0;
  double maximum_legacy_combo_score = 0;
};

// Each skill's difficulty at each object from the map's second, in time
// order (osu!'s Skill.GetObjectDifficulties). And each skill's weight: what
// its difficulty value is per unit of a difficulty every object shared,
// osu!'s weight sum for speed and reading (HarmonicSkill.ObjectWeightSum)
// and 1 / (1 - the decay weight) for aim, as osu!'s CountTopWeighted counts
// take them. The rating the map would have for a skill, were every object as
// hard as one, is that object's difficulty times the weight, rated
// (aim_rating, rating).
struct ObjectDifficulties {
  std::vector<double> aim, aim_no_sliders, speed, reading, flashlight;
  double              aim_weight = 0, speed_weight = 0, reading_weight = 0;
};

struct Difficulty {
  Attributes         attributes;
  ObjectDifficulties objects;
};

// The attributes counting the map only up to an object, at its end time
// (osu!'s TimedDifficultyAttributes), for each object in order.
struct TimedAttributes {
  double     time = 0;  // ms, the map's
  Attributes attributes;
};

inline Difficulty                   calculate(const Beatmap& map, const Mods& mods);
inline std::vector<TimedAttributes> calculate_timed(const Beatmap& map, const Mods& mods);

// osu!'s ratings of a skill's difficulty value: aim's, and speed's,
// reading's and flashlight's.
inline double aim_rating(double difficulty_value);
inline double rating(double difficulty_value);
// The star rating skills' ratings make; flashlight's with Flashlight only.
inline double star_rating(double aim, double speed, double reading,
                          std::optional<double> flashlight);

namespace detail {

// ------------------------------------------------------- .NET's semantics

// Math.Max and Math.Min: NaN wins, and +0 is above -0.
inline double net_max(double a, double b) {
  if (a != b)
    return (a > b || std::isnan(a)) ? a : b;
  return std::signbit(b) ? a : b;
}
inline double net_min(double a, double b) {
  if (a != b)
    return (a < b || std::isnan(a)) ? a : b;
  return std::signbit(a) ? a : b;
}
inline double net_clamp(double value, double min, double max) {
  if (value < min)
    return min;
  if (value > max)
    return max;
  return value;
}
// double.CompareTo: NaN below everything, equal to itself.
inline int compare(double a, double b) {
  if (a < b)
    return -1;
  if (a > b)
    return 1;
  if (a == b)
    return 0;
  if (std::isnan(a))
    return std::isnan(b) ? 0 : -1;
  return 1;
}
// Math.Round: halves to even.
inline double round_half_even(double x) { return std::nearbyint(x); }
// double.Lerp: a fused multiply-add where the hardware has one, as .NET's
// MultiplyAddEstimate is.
inline double net_lerp(double a, double b, double t) {
  return std::fma(a, 1.0 - t, b * t);
}
inline double degrees_to_radians(double degrees) {
  return (degrees * std::numbers::pi) / 180.0;
}
// List<T>.BinarySearch: an index of an equal item, or the complement of
// where it would go. compare(item, value) is item.CompareTo(value).
template <class T, class Compare>
int binary_search(const std::vector<T>& list, const T& value, Compare compare) {
  int lo = 0, hi = static_cast<int>(list.size()) - 1;
  while (lo <= hi) {
    const int i = lo + ((hi - lo) >> 1);
    const int order = compare(list[i], value);
    if (order == 0)
      return i;
    if (order < 0)
      lo = i + 1;
    else
      hi = i - 1;
  }
  return ~lo;
}

// ----------------------------- osu.Framework/Utils/Interpolation.cs (Lerp)

inline double lerp(double start, double final, double amount) {
  return start + (final - start) * amount;
}

// ------------------------- osu.Game/Rulesets/Difficulty/Utils/DiffUtils.cs

inline double bpm_to_milliseconds(double bpm, int delimiter = 4) {
  return 60000.0 / delimiter / bpm;
}
inline double milliseconds_to_bpm(double ms, int delimiter = 4) {
  return 60000.0 / (ms * delimiter);
}
inline double logistic(double x, double midpoint_offset, double multiplier,
                       double max_value = 1) {
  return max_value / (1 + std::exp(multiplier * (midpoint_offset - x)));
}
inline double logistic(double exponent, double max_value = 1) {
  return max_value / (1 + std::exp(exponent));
}
// Pow(double, double)
inline double pow(double x, double exponent) { return std::pow(x, exponent); }
// Pow(double, int)
inline double pow(double x, int exponent) {
  switch (exponent) {
    case 0: return 1;
    case 1: return x;
    case 2: return x * x;
    case 3: return x * x * x;
    case 4: return x * x * x * x;
    case 5: return x * x * x * x * x;
    default: return std::pow(x, exponent);
  }
}
template <class... T>
double norm(double p, T... values) {
  double sum = 0;
  for (const double x : {static_cast<double>(values)...})
    sum += detail::pow(x, p);
  return detail::pow(sum, 1.0 / p);
}
inline double smoothstep_bell_curve(double x) {
  x = 0.5 - std::abs(x - 0.5);
  x = net_clamp(x * 2.0, 0.0, 1.0);
  return x * x * (3.0 - 2.0 * x);
}
inline double smoothstep(double x, double start, double end) {
  x = net_clamp((x - start) / (end - start), 0.0, 1.0);
  return x * x * (3.0 - 2.0 * x);
}
inline double smootherstep(double x, double start, double end) {
  x = net_clamp((x - start) / (end - start), 0.0, 1.0);
  return x * x * x * (x * (6.0 * x - 15.0) + 10.0);
}
inline double reverse_lerp(double x, double start, double end) {
  return net_clamp((x - start) / (end - start), 0.0, 1.0);
}

// ------------------------------------------------ osu!lazer's object values

// IBeatmapDifficultyInfo.DifficultyRange
inline double difficulty_range(double difficulty) { return (difficulty - 5) / 5; }
inline double difficulty_range(double difficulty, double min, double mid,
                               double max) {
  if (difficulty > 5)
    return mid + (max - mid) * difficulty_range(difficulty);
  if (difficulty < 5)
    return mid + (mid - min) * difficulty_range(difficulty);
  return mid;
}

// LegacyRulesetExtensions.CalculateScaleFromCircleSize, with the fudge.
inline float scale_from_circle_size(float circle_size) {
  constexpr float broken_gamefield_rounding_allowance = 1.00041f;
  return static_cast<float>(1.0f - 0.7f * difficulty_range(circle_size)) / 2 *
         broken_gamefield_rounding_allowance;
}

// OsuHitObject's constants.
constexpr float  object_radius = 64;
constexpr double preempt_min = 450, preempt_mid = 1200, preempt_max = 1800;

// osu.Game/Rulesets/Objects/SliderPath.cs: PositionAt
inline Vector2 position_at(const SliderPath& path, double progress) {
  const auto&  points = path.points;
  const auto&  lengths = path.lengths;
  const double distance = lengths.empty() ? 0 : lengths.back();
  const double d = net_clamp(progress, 0, 1) * distance;
  int          i = binary_search(lengths, d, compare);
  if (i < 0)
    i = ~i;
  if (points.empty())
    return {};
  if (i <= 0)
    return points.front();
  if (i >= static_cast<int>(points.size()))
    return points.back();
  const Vector2 p0 = points[i - 1], p1 = points[i];
  const double  d0 = lengths[i - 1], d1 = lengths[i];
  // Precision.AlmostEquals
  if (std::abs(d0 - d1) <= 1e-7)
    return p0;
  const double w = (d - d0) / (d1 - d0);
  return p0 + (p1 - p0) * static_cast<float>(w);
}

// What the calculation reads of an object (OsuHitObject, Slider): the input
// with osu!lazer's ApplyDefaults and the mods' changes worked out.
struct Object {
  const HitObject* source = nullptr;
  HitObject::Kind  kind = HitObject::circle;
  double           start_time = 0, end_time = 0;
  double           radius = 0;
  Vector2          stacked_position, stacked_end_position;
  double           time_preempt = 0, time_fade_in = 0;
  double           hit_window_great = 0;  // raw ms, half the window
  // Sliders.
  int                  repeat_count = 0;
  double               duration = 0, span_duration = 0;
  std::vector<Vector2> nested_stacked;  // source->nested's, stacked

  bool is_slider() const { return kind == HitObject::slider; }
  bool is_spinner() const { return kind == HitObject::spinner; }
};

inline Object make_object(const HitObject& h, const Beatmap& map, const Mods& mods) {
  Object o;
  o.source = &h;
  o.kind = h.kind;
  o.start_time = h.start_time;
  o.end_time = h.kind == HitObject::circle ? h.start_time : h.end_time;
  const float scale = scale_from_circle_size(map.circle_size);
  o.radius = object_radius * scale;
  const float   offset = static_cast<float>(h.stack_height) * scale * -6.4f;
  const Vector2 stack_offset{offset, offset};
  o.stacked_position = h.position + stack_offset;
  o.stacked_end_position = o.stacked_position;
  o.time_preempt = static_cast<int>(difficulty_range(
      map.approach_rate, preempt_max, preempt_mid, preempt_min));
  o.time_fade_in = 400 * net_min(1, o.time_preempt / preempt_min);
  // OsuModHidden: sliders keep theirs, as in stable.
  if (mods.hidden && h.kind != HitObject::slider)
    o.time_fade_in = o.time_preempt * 0.4;
  // OsuHitWindows' great, which a slider takes from its head; a spinner has
  // none.
  if (h.kind != HitObject::spinner)
    o.hit_window_great =
        std::floor(difficulty_range(map.overall_difficulty, 80, 50, 20)) - 0.5;
  if (h.kind == HitObject::slider) {
    o.repeat_count = h.repeat_count;
    o.duration = o.end_time - o.start_time;
    o.span_duration = o.duration / (h.repeat_count + 1);
    for (const NestedObject& n : h.nested) {
      o.nested_stacked.push_back(n.position + stack_offset);
      if (n.kind == NestedObject::tail)
        o.stacked_end_position = o.nested_stacked.back();
    }
  }
  return o;
}

// ------------ osu.Game/Rulesets/Difficulty/Preprocessing/DifficultyHitObject.cs
// ---- osu.Game.Rulesets.Osu/Difficulty/Preprocessing/OsuDifficultyHitObject.cs

constexpr int   normalised_radius = 50;
constexpr int   normalised_diameter = normalised_radius * 2;
constexpr int   min_delta_time = 25;
constexpr float maximum_slider_radius = normalised_radius * 2.4f;
constexpr float assumed_slider_radius = normalised_radius * 1.8f;
constexpr double tail_leniency = -36;  // SliderEventGenerator.TAIL_LENIENCY

struct DifficultyObject {
  const std::vector<DifficultyObject>* list = nullptr;
  int                                  index = 0;
  const Object*                        base = nullptr;
  const Object*                        last = nullptr;
  double                               delta_time = 0, start_time = 0, end_time = 0;
  double                               clock_rate = 1;
  double                               hit_window_great = 0;

  double                 adjusted_delta_time = 0;
  double                 last_object_end_delta_time = 0;
  double                 jump_distance = 0, lazy_jump_distance = 0;
  double                 minimum_jump_distance = 0, minimum_jump_time = 0;
  double                 travel_distance = 0, travel_time = 0;
  std::optional<Vector2> lazy_end_position;
  double                 lazy_travel_distance = 0, lazy_travel_time = 0;
  std::optional<double>  angle, normalised_vector_angle;

  const DifficultyObject* previous(int skip = 0) const {
    const int i = index - (skip + 1);
    return i >= 0 && i < static_cast<int>(list->size()) ? &(*list)[i] : nullptr;
  }
  const DifficultyObject* next(int skip = 0) const {
    const int i = index + (skip + 1);
    return i >= 0 && i < static_cast<int>(list->size()) ? &(*list)[i] : nullptr;
  }
  double preempt() const { return base->time_preempt / clock_rate; }
  double small_circle_bonus() const {
    return net_max(1.0, 1.0 + (30 - base->radius) / 70);
  }
  double overall_difficulty() const { return (79.5 - hit_window_great / 2) / 6; }

  double opacity_at(double time, bool hidden) const {
    if (time > base->start_time)
      return 0.0;
    const double fade_in_start_time = base->start_time - base->time_preempt;
    const double fade_in_duration =
        400 * net_min(1, base->time_preempt / preempt_min);
    if (hidden) {
      const double fade_out_start_time =
          base->start_time - base->time_preempt + base->time_fade_in;
      const double fade_out_duration = base->time_preempt * 0.3;
      return net_min(
          net_clamp((time - fade_in_start_time) / fade_in_duration, 0.0, 1.0),
          1.0 - net_clamp((time - fade_out_start_time) / fade_out_duration, 0.0,
                          1.0));
    }
    return net_clamp((time - fade_in_start_time) / fade_in_duration, 0.0, 1.0);
  }

  double double_tap_feasibility(const DifficultyObject* next_object) const {
    if (!next_object)
      return 0;
    const double curr_delta_time = net_max(1, delta_time);
    const double next_delta_time = net_max(1, next_object->delta_time);
    const double delta_difference = std::abs(next_delta_time - curr_delta_time);
    const double speed_ratio =
        curr_delta_time / net_max(curr_delta_time, delta_difference);
    const double window_ratio =
        detail::pow(net_min(1, curr_delta_time / hit_window_great), 5);
    const double distance_factor = detail::pow(
        reverse_lerp(lazy_jump_distance, normalised_diameter, normalised_radius),
        2);
    return 1.0 - detail::pow(speed_ratio, distance_factor * (1 - window_ratio));
  }
};

inline Vector2 end_cursor_position(const DifficultyObject& d) {
  return d.lazy_end_position ? *d.lazy_end_position : d.base->stacked_position;
}

inline double calculate_angle(Vector2 current, Vector2 last, Vector2 last_last) {
  const Vector2 v1 = last_last - last;
  const Vector2 v2 = current - last;
  const float   dot = Vector2::dot(v1, v2);
  const float   det = v1.x * v2.y - v1.y * v2.x;
  return std::abs(std::atan2(static_cast<double>(det), static_cast<double>(dot)));
}

inline void compute_slider_cursor_position(DifficultyObject& d) {
  const Object& slider = *d.base;
  if (!slider.is_slider() || d.lazy_end_position)
    return;
  const auto& nested = slider.source->nested;
  double      tracking_end_time =
      net_max(slider.start_time + slider.duration + tail_leniency,
              slider.start_time + slider.duration / 2);
  // The order the cursor passes the nested objects in, as indices.
  std::vector<size_t> order(nested.size());
  for (size_t i = 0; i < order.size(); ++i)
    order[i] = i;
  std::optional<size_t> last_real_tick;
  for (size_t i = 0; i < nested.size(); ++i)
    if (nested[i].kind == NestedObject::tick)
      last_real_tick = i;
  if (last_real_tick && nested[*last_real_tick].time > tracking_end_time) {
    tracking_end_time = nested[*last_real_tick].time;
    // Moved to the end, keeping osu!'s known output.
    order.erase(order.begin() + static_cast<std::ptrdiff_t>(*last_real_tick));
    order.push_back(*last_real_tick);
  }
  d.lazy_travel_time = tracking_end_time - slider.start_time;
  double end_time_min = d.lazy_travel_time / slider.span_duration;
  if (std::fmod(end_time_min, 2) >= 1)
    end_time_min = 1 - std::fmod(end_time_min, 1);
  else
    end_time_min = std::fmod(end_time_min, 1);
  d.lazy_end_position =
      slider.stacked_position + position_at(slider.source->path, end_time_min);
  Vector2      curr_cursor_position = slider.stacked_position;
  const double scaling_factor = normalised_radius / slider.radius;
  for (size_t i = 1; i < order.size(); ++i) {
    const NestedObject& movement_object = nested[order[i]];
    Vector2 curr_movement = slider.nested_stacked[order[i]] - curr_cursor_position;
    double  curr_movement_length = scaling_factor * curr_movement.length();
    double  required_movement = assumed_slider_radius;
    if (i == order.size() - 1) {
      const Vector2 lazy_movement = *d.lazy_end_position - curr_cursor_position;
      if (lazy_movement.length() < curr_movement.length())
        curr_movement = lazy_movement;
      curr_movement_length = scaling_factor * curr_movement.length();
    } else if (movement_object.kind == NestedObject::repeat) {
      required_movement = normalised_radius;
    }
    if (curr_movement_length > required_movement) {
      curr_cursor_position =
          curr_cursor_position +
          curr_movement * static_cast<float>((curr_movement_length - required_movement) /
                                             curr_movement_length);
      curr_movement_length *=
          (curr_movement_length - required_movement) / curr_movement_length;
      d.lazy_travel_distance += curr_movement_length;
    }
    if (i == order.size() - 1)
      d.lazy_end_position = curr_cursor_position;
  }
}

inline double calculate_slider_angle(const DifficultyObject& d,
                                     const DifficultyObject& last,
                                     Vector2                 last_last_cursor) {
  const Vector2 last_cursor = end_cursor_position(last);
  if (last.base->is_slider() && last.travel_distance > 0)
    last_last_cursor = last.base->nested_stacked[last.base->nested_stacked.size() - 2];
  return calculate_angle(d.base->stacked_position, last_cursor, last_last_cursor);
}

inline void set_distances(DifficultyObject& d, double clock_rate) {
  const Object& base = *d.base;
  if (base.is_slider()) {
    d.travel_distance =
        d.lazy_travel_distance * net_max(1, detail::pow(static_cast<double>(base.repeat_count), 0.3));
    d.travel_time = net_max(d.lazy_travel_time / clock_rate, min_delta_time);
  }
  d.minimum_jump_time = d.adjusted_delta_time;
  if (base.is_spinner() || d.last->is_spinner())
    return;
  const float             scaling_factor = normalised_radius / static_cast<float>(base.radius);
  const DifficultyObject* last = d.previous();
  const DifficultyObject* last_last = d.previous(1);
  Vector2 last_cursor = last ? end_cursor_position(*last) : d.last->stacked_position;
  d.jump_distance = (d.last->stacked_position - base.stacked_position).length() * scaling_factor;
  d.lazy_jump_distance = (base.stacked_position - last_cursor).length() * scaling_factor;
  d.minimum_jump_distance = d.lazy_jump_distance;
  if (d.last->is_slider() && last) {
    const double last_travel_time =
        net_max(last->lazy_travel_time / clock_rate, min_delta_time);
    d.minimum_jump_time =
        net_max(d.adjusted_delta_time - last_travel_time, min_delta_time);
    const float tail_jump_distance =
        (d.last->stacked_end_position - base.stacked_position).length() * scaling_factor;
    d.minimum_jump_distance = net_max(
        0, net_min(d.lazy_jump_distance - (maximum_slider_radius - assumed_slider_radius),
                   tail_jump_distance - maximum_slider_radius));
  }
  if (last_last && !last_last->base->is_spinner()) {
    if (last->base->is_slider() && last->travel_distance > 0)
      last_cursor = last->base->stacked_position;  // its head's
    const Vector2 last_last_cursor = end_cursor_position(*last_last);
    const double  angle =
        calculate_angle(base.stacked_position, last_cursor, last_last_cursor);
    const double  slider_angle = calculate_slider_angle(d, *last, last_last_cursor);
    const Vector2 v = base.stacked_position - last_cursor;
    d.normalised_vector_angle = std::atan2(static_cast<double>(std::abs(v.y)),
                                           static_cast<double>(std::abs(v.x)));
    d.angle = net_min(angle, slider_angle);
  }
}

inline DifficultyObject make_difficulty_object(const Object& h, const Object& last,
                                               double clock_rate,
                                               const std::vector<DifficultyObject>& list,
                                               int index) {
  DifficultyObject d;
  d.list = &list;
  d.index = index;
  d.base = &h;
  d.last = &last;
  d.delta_time = (h.start_time - last.start_time) / clock_rate;
  d.start_time = h.start_time / clock_rate;
  d.end_time = h.end_time / clock_rate;
  d.clock_rate = clock_rate;
  d.hit_window_great = 2 * h.hit_window_great / clock_rate;
  d.adjusted_delta_time = net_max(d.delta_time, min_delta_time);
  const DifficultyObject* previous = d.previous();
  d.last_object_end_delta_time =
      previous ? net_max(d.start_time - previous->end_time, min_delta_time)
               : d.adjusted_delta_time;
  compute_slider_cursor_position(d);
  set_distances(d, clock_rate);
  return d;
}

// ---- osu.Game.Rulesets.Osu/Difficulty/Evaluators/Aim/AgilityEvaluator.cs

inline double agility_difficulty(const DifficultyObject& current) {
  if (current.base->is_spinner())
    return 0;
  constexpr double distance_cap = normalised_diameter * 1.2;
  const DifficultyObject* prev = current.index > 0 ? current.previous() : nullptr;
  const double travel_distance = prev ? prev->lazy_travel_distance : 0;
  const double distance = travel_distance + current.lazy_jump_distance;
  const double distance_scaled = net_min(distance, distance_cap) / distance_cap;
  double difficulty = distance_scaled * 1000 / current.adjusted_delta_time;
  difficulty *= detail::pow(current.small_circle_bonus(), 1.5);
  difficulty *= 1 / (1 - detail::pow(0.2, current.adjusted_delta_time / 1000));
  return difficulty;
}

// ---- osu.Game.Rulesets.Osu/Difficulty/Evaluators/Aim/SnapAimEvaluator.cs

inline double angle_wideness(double angle) {
  return smoothstep(angle, degrees_to_radians(40), degrees_to_radians(140));
}
inline double angle_acuteness(double angle) {
  return smoothstep(angle, degrees_to_radians(140), degrees_to_radians(40));
}

inline double vector_angle_repetition(const DifficultyObject& current,
                                      const DifficultyObject& previous) {
  if (!current.angle || !previous.angle)
    return 1;
  constexpr double note_limit = 6;
  constexpr double maximum_repetition_nerf = 0.15;
  constexpr double maximum_vector_influence = 0.5;
  double constant_angle_count = 0;
  for (int index = 0; index < note_limit; ++index) {
    const DifficultyObject* prev = current.previous(index);
    if (!prev)
      break;
    if (net_max(current.adjusted_delta_time, prev->adjusted_delta_time) >
        1.1 * net_min(current.adjusted_delta_time, prev->adjusted_delta_time))
      break;
    if (prev->normalised_vector_angle && current.normalised_vector_angle) {
      const double angle_difference =
          std::abs(*current.normalised_vector_angle - *prev->normalised_vector_angle);
      constant_angle_count +=
          std::cos(8 * net_min(degrees_to_radians(11.25), angle_difference));
    }
  }
  const double vector_repetition = detail::pow(net_min(0.5 / constant_angle_count, 1), 2);
  const double stack_factor =
      smootherstep(current.lazy_jump_distance, 0, normalised_diameter);
  const double curr_angle = *current.angle;
  const double last_angle = *previous.angle;
  const double angle_difference_adjusted = std::cos(
      2 * net_min(degrees_to_radians(45), std::abs(curr_angle - last_angle) * stack_factor));
  const double base_nerf = 1 - maximum_repetition_nerf * angle_acuteness(last_angle) *
                                   angle_difference_adjusted;
  return detail::pow(base_nerf + (1 - base_nerf) * vector_repetition *
                                     maximum_vector_influence * stack_factor,
                     2);
}

inline double snap_aim_difficulty(const DifficultyObject& current,
                                  bool with_slider_travel_distance) {
  const DifficultyObject* last = current.previous();
  if (current.base->is_spinner() || current.index <= 1 || last->base->is_spinner())
    return 0;
  constexpr double wide_angle_multiplier = 9.67;
  constexpr double acute_angle_multiplier = 2.41;
  constexpr double slider_multiplier = 1.5;
  constexpr double velocity_change_multiplier = 0.9;
  constexpr double wiggle_multiplier = 1.02;
  const DifficultyObject* last2 = current.previous(2);
  constexpr int radius = normalised_radius;
  constexpr int diameter = normalised_diameter;

  const double curr_distance = with_slider_travel_distance ? current.lazy_jump_distance
                                                           : current.jump_distance;
  double curr_velocity = curr_distance / current.adjusted_delta_time;
  if (last->base->is_slider() && with_slider_travel_distance) {
    const double slider_distance = last->lazy_travel_distance + current.lazy_jump_distance;
    curr_velocity = net_max(curr_velocity, slider_distance / current.adjusted_delta_time);
  }
  const double prev_distance =
      with_slider_travel_distance ? last->lazy_jump_distance : last->jump_distance;
  const double prev_velocity = prev_distance / last->adjusted_delta_time;
  double       snap = curr_velocity;
  snap *= vector_angle_repetition(current, *last);

  if (current.angle && last->angle) {
    const double curr_angle = *current.angle;
    const double last_angle = *last->angle;
    const double velocity_influence = net_min(curr_velocity, prev_velocity);
    double       acute_angle_bonus = 0;
    if (net_max(current.adjusted_delta_time, last->adjusted_delta_time) <
        1.25 * net_min(current.adjusted_delta_time, last->adjusted_delta_time)) {
      acute_angle_bonus = angle_acuteness(curr_angle);
      acute_angle_bonus *= 0.08 + 0.92 * (1 - net_min(acute_angle_bonus,
                                                       detail::pow(angle_acuteness(last_angle), 3)));
      acute_angle_bonus *=
          velocity_influence *
          smootherstep(milliseconds_to_bpm(current.adjusted_delta_time, 2), 300, 400) *
          smootherstep(curr_distance, 0, diameter * 2);
    }
    double wide_angle_bonus = angle_wideness(curr_angle);
    wide_angle_bonus *=
        0.25 + 0.75 * (1 - net_min(wide_angle_bonus, detail::pow(angle_wideness(last_angle), 3)));
    constexpr double wide_angle_time_scale = 1.45;
    double wide_curr_velocity =
        curr_distance / detail::pow(current.adjusted_delta_time, wide_angle_time_scale);
    const double wide_prev_velocity =
        prev_distance / detail::pow(last->adjusted_delta_time, wide_angle_time_scale);
    if (last->base->is_slider() && with_slider_travel_distance) {
      const double slider_distance = last->lazy_travel_distance + current.lazy_jump_distance;
      wide_curr_velocity = net_max(
          wide_curr_velocity,
          slider_distance / detail::pow(current.adjusted_delta_time, wide_angle_time_scale));
    }
    wide_angle_bonus *= net_min(wide_curr_velocity, wide_prev_velocity);
    if (last2) {
      const float distance =
          (last2->base->stacked_position - last->base->stacked_position).length();
      if (distance < 1)
        wide_angle_bonus *= 1 - 0.55 * (1 - distance);
    }
    snap += net_max(acute_angle_bonus * acute_angle_multiplier,
                    wide_angle_bonus * wide_angle_multiplier);
    const double wiggle_bonus =
        velocity_influence * smootherstep(curr_distance, radius, diameter) *
        detail::pow(reverse_lerp(curr_distance, diameter * 3, diameter), 1.8) *
        smootherstep(curr_angle, degrees_to_radians(110), degrees_to_radians(60)) *
        smootherstep(prev_distance, radius, diameter) *
        detail::pow(reverse_lerp(prev_distance, diameter * 3, diameter), 1.8) *
        smootherstep(last_angle, degrees_to_radians(110), degrees_to_radians(60));
    snap += wiggle_bonus * wiggle_multiplier;
  }

  if (net_max(prev_velocity, curr_velocity) != 0) {
    if (with_slider_travel_distance)
      curr_velocity = curr_distance / current.adjusted_delta_time;
    const double dist_ratio = smoothstep(
        std::abs(prev_velocity - curr_velocity) / net_max(prev_velocity, curr_velocity), 0, 1);
    const double overlap_velocity_buff =
        net_min(diameter * 1.25 /
                    net_min(current.adjusted_delta_time, last->adjusted_delta_time),
                std::abs(prev_velocity - curr_velocity));
    double velocity_change_bonus = overlap_velocity_buff * dist_ratio;
    velocity_change_bonus *=
        detail::pow(net_min(current.adjusted_delta_time, last->adjusted_delta_time) /
                        net_max(current.adjusted_delta_time, last->adjusted_delta_time),
                    2);
    snap += velocity_change_bonus * velocity_change_multiplier;
  }

  if (current.base->is_slider() && with_slider_travel_distance) {
    const double slider_bonus = current.travel_distance / current.travel_time;
    snap += (slider_bonus < 1 ? slider_bonus : detail::pow(slider_bonus, 0.75)) *
            slider_multiplier;
  }
  snap *= current.small_circle_bonus();
  snap *= 1 / (1 - detail::pow(0.03, detail::pow(current.adjusted_delta_time / 1000, 0.65)));
  return snap;
}

// ---- osu.Game.Rulesets.Osu/Difficulty/Evaluators/Aim/FlowAimEvaluator.cs

inline double overlap_factor(const DifficultyObject& first, const DifficultyObject& second) {
  const double object_radius = first.base->radius;
  const double distance =
      Vector2::distance(first.base->stacked_position, second.base->stacked_position);
  return net_clamp(
      1 - detail::pow(net_max(distance - object_radius, 0) / object_radius, 2), 0, 1);
}

inline double flow_aim_difficulty(const DifficultyObject& current,
                                  bool with_slider_travel_distance) {
  const DifficultyObject* last = current.previous();
  if (current.base->is_spinner() || current.index <= 1 || last->base->is_spinner())
    return 0;
  constexpr double velocity_change_multiplier = 0.52;
  const DifficultyObject* last_last = current.previous(1);
  const double curr_distance = with_slider_travel_distance ? current.lazy_jump_distance
                                                           : current.jump_distance;
  const double prev_distance =
      with_slider_travel_distance ? last->lazy_jump_distance : last->jump_distance;
  double curr_velocity = curr_distance / current.adjusted_delta_time;
  if (last->base->is_slider() && with_slider_travel_distance) {
    const double slider_distance = last->lazy_travel_distance + current.lazy_jump_distance;
    curr_velocity = net_max(curr_velocity, slider_distance / current.adjusted_delta_time);
  }
  const double prev_velocity = prev_distance / last->adjusted_delta_time;
  double       flow = curr_velocity;
  flow *= std::sqrt(current.small_circle_bonus());
  flow *= 1 + net_min(0.25, detail::pow((net_max(current.adjusted_delta_time, last->adjusted_delta_time) -
                                         net_min(current.adjusted_delta_time, last->adjusted_delta_time)) /
                                            50,
                                        4));
  if (current.angle && last->angle) {
    const double angle_difference = std::abs(*current.angle - *last->angle);
    const double angle_difference_adjusted = std::sin(angle_difference / 2) * 180.0;
    const double angular_velocity =
        angle_difference_adjusted / (current.adjusted_delta_time * 0.1);
    flow *= 0.8 + std::sqrt(angular_velocity / 270.0);
  }
  double overlapped_notes_weight = 1;
  if (current.index > 2) {
    const double o1 = overlap_factor(current, *last);
    const double o2 = overlap_factor(current, *last_last);
    const double o3 = overlap_factor(*last, *last_last);
    overlapped_notes_weight = 1 - o1 * o2 * o3;
  }
  if (current.angle)
    flow += curr_velocity * angle_acuteness(*current.angle) * overlapped_notes_weight;
  if (net_max(prev_velocity, curr_velocity) != 0) {
    if (with_slider_travel_distance)
      curr_velocity = curr_distance / current.adjusted_delta_time;
    const double dist_ratio = smoothstep(
        std::abs(prev_velocity - curr_velocity) / net_max(prev_velocity, curr_velocity), 0, 1);
    const double overlap_velocity_buff =
        net_min(normalised_diameter * 1.25 /
                    net_min(current.adjusted_delta_time, last->adjusted_delta_time),
                std::abs(prev_velocity - curr_velocity));
    flow += overlap_velocity_buff * dist_ratio * overlapped_notes_weight *
            velocity_change_multiplier;
  }
  if (current.base->is_slider() && with_slider_travel_distance)
    flow += current.travel_distance / current.travel_time;
  flow = detail::pow(flow, 1.45);
  return flow * smootherstep(curr_distance, 0, normalised_radius);
}

// ---- osu.Game.Rulesets.Osu/Difficulty/Evaluators/Speed/SpeedEvaluator.cs

inline double speed_difficulty(const DifficultyObject& current) {
  if (current.base->is_spinner())
    return 0;
  constexpr double min_speed_bonus = 200;
  constexpr double speed_balancing_factor = 40;
  double       strain_time = current.adjusted_delta_time;
  const double double_tap_feasibility = 1.0 - current.double_tap_feasibility(current.next());
  strain_time /= net_clamp((strain_time / current.hit_window_great) / 0.93, 0.92, 1);
  double speed_bonus = 0.0;
  if (milliseconds_to_bpm(strain_time) > min_speed_bonus)
    speed_bonus = 0.75 * detail::pow((bpm_to_milliseconds(min_speed_bonus) - strain_time) /
                                         speed_balancing_factor,
                                     2);
  double difficulty = (1 + speed_bonus) * 1000 / strain_time;
  difficulty *= 1 / (1 - detail::pow(0.3, current.adjusted_delta_time / 1000));
  return difficulty * double_tap_feasibility;
}

// ---- osu.Game.Rulesets.Osu/Difficulty/Evaluators/Speed/RhythmEvaluator.cs

struct Island {
  int delta = 0;
  int delta_count = 1;
  int occurrences = 1;

  explicit Island(int d) : delta(std::max(d, min_delta_time)) {}
  void add_delta(int d) {
    if (delta == INT32_MAX)
      delta = std::max(d, min_delta_time);
    ++delta_count;
  }
  bool is_similar_polarity(const Island& other, double epsilon) const {
    if (delta_count <= 1 || other.delta_count <= 1)
      return false;
    return std::abs(delta - other.delta) < epsilon &&
           delta_count % 2 == other.delta_count % 2;
  }
  bool almost_equals(const Island& other, double epsilon) const {
    return std::abs(delta - other.delta) < epsilon && delta_count == other.delta_count;
  }
};

inline double effective_difficulty(double delta_difference_ratio) {
  constexpr double rhythm_ratio_difficulty_multiplier = 26.0;
  const double fraction = delta_difference_ratio - std::trunc(delta_difference_ratio);
  return 1.0 + rhythm_ratio_difficulty_multiplier *
                   net_min(0.5, smoothstep_bell_curve(fraction));
}

inline double rhythm_difficulty(const DifficultyObject& current) {
  if (current.base->is_spinner())
    return 0;
  constexpr int    history_time_max = 5 * 1000;
  constexpr int    history_objects_max = 32;
  constexpr double rhythm_overall_multiplier = 0.95;
  double           rhythm_complexity_sum = 0;
  const double     delta_difference_epsilon = current.hit_window_great * 0.3;
  // Islands are shared, as osu!'s objects are: one may be the current one,
  // the previous one and in the list at once.
  std::vector<std::unique_ptr<Island>> pool;
  const auto make = [&](int delta) {
    pool.push_back(std::make_unique<Island>(delta));
    return pool.back().get();
  };
  Island*              island = make(INT32_MAX);
  Island*              previous_island = make(INT32_MAX);
  std::vector<Island*> islands;
  double               start_difficulty = 0;
  bool                 first_delta_switch = false;
  const int            historical_note_count = std::min(current.index, history_objects_max);
  int                  rhythm_start = 0;
  while (rhythm_start < historical_note_count - 2 &&
         current.start_time - current.previous(rhythm_start)->start_time < history_time_max)
    ++rhythm_start;
  const DifficultyObject* prev = current.previous(rhythm_start);
  const DifficultyObject* prev_prev = current.previous(rhythm_start + 1);
  for (int i = rhythm_start; i > 0; --i) {
    const DifficultyObject* curr = current.previous(i - 1);
    if (curr->base->is_spinner())
      continue;
    const double time_decay =
        (history_time_max - (current.start_time - curr->start_time)) / history_time_max;
    const double note_decay =
        static_cast<double>(historical_note_count - i) / historical_note_count;
    const double     curr_historical_decay = net_min(note_decay, time_decay);
    constexpr double delta_min_value = 1e-7;
    const double     curr_delta = net_max(curr->delta_time, delta_min_value);
    const double     prev_delta = net_max(prev->delta_time, delta_min_value);
    const double     delta_difference = std::abs(prev_delta - curr_delta);
    if (island->delta == INT32_MAX)
      island = make(static_cast<int>(curr_delta));
    const double delta_difference_ratio =
        net_max(prev_delta, curr_delta) / net_min(prev_delta, curr_delta);
    const double difference_multiplier =
        net_clamp(2.0 - delta_difference_ratio / 8.0, 0.0, 1.0);
    const double window_penalty = net_clamp(
        (delta_difference - delta_difference_epsilon) / delta_difference_epsilon, 0, 1);
    double effective = effective_difficulty(delta_difference_ratio) * window_penalty *
                       difference_multiplier;
    if (prev->base->is_slider()) {
      const double slider_lazy_end_delta = curr->minimum_jump_time;
      const double slider_lazy_ratio = net_max(slider_lazy_end_delta, curr_delta) /
                                       net_min(slider_lazy_end_delta, curr_delta);
      const double slider_real_end_delta = curr->last_object_end_delta_time;
      const double slider_real_ratio = net_max(slider_real_end_delta, curr_delta) /
                                       net_min(slider_real_end_delta, curr_delta);
      const double slider_effective = net_min(effective_difficulty(slider_lazy_ratio),
                                              effective_difficulty(slider_real_ratio));
      effective = net_min(slider_effective, effective);
    }
    if (delta_difference < delta_difference_epsilon)
      island->add_delta(static_cast<int>(curr_delta));
    if (first_delta_switch) {
      if (delta_difference > delta_difference_epsilon) {
        if (curr->base->is_slider())
          effective *= 0.5;
        if (island->is_similar_polarity(*previous_island, delta_difference_epsilon))
          effective *= 0.5;
        if (net_max(prev_prev->delta_time, delta_min_value) >
                prev_delta + delta_difference_epsilon &&
            prev_delta > curr_delta + delta_difference_epsilon)
          effective *= 0.125;
        if (previous_island->delta_count == island->delta_count)
          effective *= 0.5;
        const bool is_speeding_up = prev_delta > curr_delta + delta_difference_epsilon;
        if (is_speeding_up)
          effective *= 0.65;
        bool found = false;
        for (Island* existing : islands) {
          if (existing->almost_equals(*island, delta_difference_epsilon)) {
            if (previous_island->almost_equals(*island, delta_difference_epsilon))
              ++existing->occurrences;
            const double power = logistic(island->delta, 58.33, 0.24, 2.75);
            effective *= net_min(3.0 / existing->occurrences,
                                 detail::pow(1.0 / existing->occurrences, power));
            found = true;
            break;
          }
        }
        if (!found && island->delta_count > 0)
          islands.push_back(island);
        effective *= 1 - prev->double_tap_feasibility(curr) * 0.75;
        if (island->delta_count > 1)
          rhythm_complexity_sum += std::sqrt(effective * start_difficulty) * curr_historical_decay;
        else
          rhythm_complexity_sum += 0.7 * curr_historical_decay;
        start_difficulty = effective;
        if (prev_delta + delta_difference_epsilon < curr_delta)
          first_delta_switch = false;
        previous_island = island;
        island = make(static_cast<int>(curr_delta));
      }
    } else if (prev_delta > curr_delta + delta_difference_epsilon) {
      first_delta_switch = true;
      if (curr->base->is_slider())
        effective *= 0.6;
      if (prev->base->is_slider())
        effective *= 0.6;
      start_difficulty = effective;
      island = make(static_cast<int>(curr_delta));
    }
    prev_prev = prev;
    prev = curr;
  }
  rhythm_complexity_sum *= reverse_lerp(island->delta_count, 22, 3);
  return std::sqrt(4 + rhythm_complexity_sum * rhythm_overall_multiplier) / 2.0;
}

// ---- osu.Game.Rulesets.Osu/Difficulty/Evaluators/ReadingEvaluator.cs

constexpr double reading_window_size = 3000;
constexpr double distance_influence_threshold = normalised_diameter * 1.5;

inline double time_nerf_factor(double delta_time) {
  return net_clamp(2 - delta_time / (reading_window_size / 2), 0, 1);
}

inline double current_visible_object_density(const DifficultyObject& current) {
  double                  visible_object_count = 0;
  const DifficultyObject* object = current.next();
  while (object) {
    if (object->start_time - current.start_time > reading_window_size ||
        current.start_time < object->start_time - object->preempt())
      break;
    const double time_between = object->start_time - current.start_time;
    visible_object_count +=
        object->opacity_at(current.base->start_time, false) * time_nerf_factor(time_between);
    object = object->next();
  }
  return visible_object_count;
}

inline double past_object_difficulty_influence(const DifficultyObject& current) {
  double influence = 0;
  for (int i = 0; i < current.index; ++i) {
    const DifficultyObject* object = current.previous(i);
    if (!object || current.start_time - object->start_time > reading_window_size ||
        object->start_time < current.start_time - current.preempt())
      break;
    double difficulty = current.opacity_at(object->base->start_time, false);
    difficulty *= smootherstep(object->lazy_jump_distance, 15, distance_influence_threshold);
    difficulty *= time_nerf_factor(current.start_time - object->start_time);
    influence += difficulty;
  }
  return influence;
}

inline double constant_angle_nerf_factor(const DifficultyObject& current) {
  constexpr double minimum_angle_relevancy_time = 2000;
  constexpr double maximum_angle_relevancy_time = 200;
  double                  constant_angle_count = 0;
  int                     index = 0;
  double                  current_time_gap = 0;
  const DifficultyObject* prev0 = &current;
  const DifficultyObject* prev1 = nullptr;
  const DifficultyObject* prev2 = nullptr;
  while (current_time_gap < minimum_angle_relevancy_time) {
    const DifficultyObject* object = current.previous(index);
    if (!object)
      break;
    const double long_interval_factor =
        1 - reverse_lerp(object->adjusted_delta_time, maximum_angle_relevancy_time,
                         minimum_angle_relevancy_time);
    if (object->angle && current.angle) {
      const double angle_difference = std::abs(*current.angle - *object->angle);
      double       angle_difference_alternating = std::numbers::pi;
      if (prev0->angle && prev1 && prev1->angle && prev2 && prev2->angle) {
        angle_difference_alternating = std::abs(*prev1->angle - *object->angle);
        angle_difference_alternating += std::abs(*prev2->angle - *prev0->angle);
        double weight = 1.0;
        weight *= reverse_lerp(net_min(*object->angle, *prev0->angle) * 180 / std::numbers::pi, 20, 5);
        weight *= reverse_lerp(net_max(*object->angle, *prev0->angle) * 180 / std::numbers::pi, 60, 120);
        angle_difference_alternating =
            net_lerp(std::numbers::pi, 0.1 * angle_difference_alternating, weight);
      }
      const double stack_factor =
          smootherstep(object->lazy_jump_distance, 0, normalised_radius);
      constant_angle_count +=
          std::cos(3 * net_min(degrees_to_radians(30),
                               net_min(angle_difference, angle_difference_alternating) *
                                   stack_factor)) *
          long_interval_factor;
    }
    current_time_gap = current.start_time - object->start_time;
    ++index;
    prev2 = prev1;
    prev1 = prev0;
    prev0 = object;
  }
  return net_clamp(2 / constant_angle_count, 0.2, 1);
}

inline double density_difficulty(const DifficultyObject* next, double velocity,
                                 double constant_angle_nerf, double past_influence,
                                 double current_density) {
  constexpr double density_multiplier = 2.4;
  constexpr double density_difficulty_base = 2.5;
  double future_influence = std::sqrt(current_density);
  if (next)
    future_influence *= smootherstep(next->lazy_jump_distance, 15, distance_influence_threshold);
  double difficulty = detail::pow(past_influence + future_influence, 1.7) * 0.4 *
                      constant_angle_nerf * velocity;
  difficulty = net_max(0, difficulty - density_difficulty_base);
  return detail::pow(difficulty, 0.45) * density_multiplier;
}

inline double preempt_difficulty(double velocity, double constant_angle_nerf, double preempt) {
  constexpr double preempt_balancing_factor = 140000;
  constexpr double preempt_starting_point = 500;
  double difficulty = detail::pow(
      (preempt_starting_point - preempt + std::abs(preempt - preempt_starting_point)) / 2,
      2.5) / preempt_balancing_factor;
  difficulty *= constant_angle_nerf * velocity;
  return difficulty;
}

inline double hidden_difficulty(const DifficultyObject& current, double past_influence,
                                double current_density, double velocity,
                                double constant_angle_nerf) {
  constexpr double hidden_multiplier = 0.28;
  const double preempt_factor = detail::pow(current.preempt(), 2.2) * 0.01;
  const double density_factor = detail::pow(current_density + past_influence, 3.3) * 3;
  double difficulty =
      (preempt_factor + density_factor) * constant_angle_nerf * velocity * 0.01;
  difficulty = detail::pow(difficulty, 0.4) * hidden_multiplier;
  const DifficultyObject* previous = current.previous();
  if (current.lazy_jump_distance == 0 &&
      current.opacity_at(previous->base->start_time, true) == 0 &&
      previous->start_time > current.start_time - current.preempt())
    difficulty += hidden_multiplier * 2500 / detail::pow(current.adjusted_delta_time, 1.5);
  return difficulty;
}

inline double reading_difficulty(const DifficultyObject& current, bool hidden) {
  if (current.base->is_spinner() || current.index == 0)
    return 0;
  const DifficultyObject* next = current.next();
  const double velocity = net_max(1, current.lazy_jump_distance / current.adjusted_delta_time);
  const double density = current_visible_object_density(current);
  const double past = past_object_difficulty_influence(current);
  const double angle_nerf = constant_angle_nerf_factor(current);
  const double note_density = density_difficulty(next, velocity, angle_nerf, past, density);
  const double hidden_value =
      hidden ? hidden_difficulty(current, past, density, velocity, angle_nerf) : 0;
  const double preempt_value = preempt_difficulty(velocity, angle_nerf, current.preempt());
  double difficulty = norm(1.5, preempt_value, hidden_value, note_density);
  difficulty *= 1 / (1 - detail::pow(0.8, current.adjusted_delta_time / 1000));
  return difficulty;
}

// ---- osu.Game.Rulesets.Osu/Difficulty/Evaluators/FlashlightEvaluator.cs

inline double flashlight_difficulty(const DifficultyObject& current, const Mods& mods) {
  if (current.base->is_spinner())
    return 0;
  constexpr double max_opacity_bonus = 0.4;
  constexpr double hidden_bonus = 0.2;
  constexpr double min_velocity = 0.5;
  constexpr double slider_multiplier = 1.3;
  constexpr double min_angle_multiplier = 0.2;
  const Object&    object = *current.base;
  const double     scaling_factor = 52.0 / object.radius;
  const bool full_hidden = mods.hidden && !mods.hidden_only_fades_approach_circles;
  double                  small_dist_nerf = 1.0;
  double                  cumulative_strain_time = 0.0;
  double                  difficulty = 0.0;
  const DifficultyObject* last = &current;
  double                  angle_repeat_count = 0.0;
  for (int i = 0; i < std::min(current.index, 10); ++i) {
    const DifficultyObject* obj = current.previous(i);
    cumulative_strain_time += last->adjusted_delta_time;
    if (!obj->base->is_spinner()) {
      const double jump_distance =
          (object.stacked_position - obj->base->stacked_end_position).length();
      if (i == 0)
        small_dist_nerf = net_min(1.0, jump_distance / 75.0);
      const double stack_nerf = net_min(1.0, (obj->lazy_jump_distance / scaling_factor) / 25.0);
      const double opacity_bonus =
          1.0 + max_opacity_bonus *
                    (1.0 - current.opacity_at(obj->base->start_time, full_hidden));
      difficulty += stack_nerf * opacity_bonus * scaling_factor * jump_distance /
                    cumulative_strain_time;
      if (obj->angle && current.angle) {
        if (std::abs(*obj->angle - *current.angle) < 0.02)
          angle_repeat_count += net_max(1.0 - 0.1 * i, 0.0);
      }
    }
    last = obj;
  }
  difficulty = detail::pow(small_dist_nerf * difficulty, 2);
  if (mods.hidden)
    difficulty *= 1.0 + hidden_bonus;
  difficulty *= min_angle_multiplier + (1.0 - min_angle_multiplier) / (angle_repeat_count + 1.0);
  double slider_bonus = 0.0;
  if (object.is_slider()) {
    const double pixel_travel_distance = current.lazy_travel_distance / scaling_factor;
    slider_bonus = detail::pow(
        net_max(0.0, pixel_travel_distance / current.travel_time - min_velocity), 0.5);
    slider_bonus *= pixel_travel_distance;
    if (object.repeat_count > 0)
      slider_bonus /= (object.repeat_count + 1);
  }
  difficulty += slider_bonus * slider_multiplier;
  return difficulty;
}

// ---------------------- osu.Game/Rulesets/Difficulty/Skills/Skill.cs, and
// ---------------------- osu.Game/Rulesets/Difficulty/Skills/HarmonicSkill.cs

struct HarmonicSkill {
  std::vector<double> object_difficulties;
  double              object_weight_sum = 0;
  double              harmonic_scale = 1.0;
  double              decay_exponent = 0.9;

  double harmonic_value(std::vector<double> difficulties) {
    object_weight_sum = 0;
    if (object_difficulties.empty())
      return 0;
    std::stable_sort(difficulties.begin(), difficulties.end(),
                     [](double a, double b) { return compare(a, b) > 0; });
    double difficulty = 0;
    int    index = 0;
    for (const double object : difficulties) {
      if (!(object > 0))
        continue;
      const double weight =
          (1 + (harmonic_scale / (1 + index))) /
          (detail::pow(static_cast<double>(index), decay_exponent) + 1 +
           (harmonic_scale / (1 + index)));
      object_weight_sum += weight;
      difficulty += object * weight;
      index += 1;
    }
    return difficulty;
  }
  double count_top_weighted(double difficulty_value, double midpoint, double multiplier) const {
    if (object_difficulties.empty() || object_weight_sum == 0)
      return 0.0;
    const double consistent_top = difficulty_value / object_weight_sum;
    if (consistent_top == 0)
      return 0;
    double sum = 0;
    for (const double d : object_difficulties)
      sum += logistic(d / consistent_top, midpoint, multiplier, 1.1);
    return sum;
  }
};

inline double harmonic_difficulty_to_performance(double difficulty) {
  return 4.0 * detail::pow(difficulty, 3);
}

// ------- osu.Game/Rulesets/Difficulty/Skills/VariableLengthStrainSkill.cs

struct StrainPeak {
  double value;
  double section_length;
  StrainPeak(double v, double length) : value(v), section_length(round_half_even(length)) {}
  bool operator==(const StrainPeak&) const = default;
};
// Highest first.
inline int compare_peaks(const StrainPeak& a, const StrainPeak& b) {
  return compare(b.value, a.value);
}

struct VariableLengthStrainSkill {
  std::vector<double>       object_difficulties;
  double                    decay_weight = 0.9;
  int                       max_section_length = 400;
  double                    current_section_peak = 0;
  double                    current_section_begin = 0;
  double                    current_section_end = 0;
  double                    max_stored_length = 11 / (1 - decay_weight);
  std::vector<StrainPeak>   strain_peaks;
  double                    total_length = 0;
  std::vector<std::pair<double, double>> queued_strains;  // strain, start time
  std::optional<StrainPeak> final_peak;

  template <class StrainAt, class InitialStrain>
  double process(const DifficultyObject& current, StrainAt strain_at,
                 InitialStrain initial_strain) {
    if (current.index == 0) {
      current_section_begin = current.start_time;
      current_section_end = current_section_begin + max_section_length;
      current_section_peak = strain_at(current);
      return current_section_peak;
    }
    backfill_peaks(current, initial_strain);
    const double current_strain = strain_at(current);
    if (current_strain > current_section_peak) {
      queued_strains.clear();
      save_current_peak(current.start_time - current_section_begin);
      current_section_begin = current.start_time;
      current_section_end = current_section_begin + max_section_length;
      current_section_peak = current_strain;
    } else {
      while (!queued_strains.empty() && queued_strains.back().first < current_strain)
        queued_strains.pop_back();
      queued_strains.emplace_back(current_strain, current.start_time);
    }
    return current_strain;
  }

  template <class InitialStrain>
  void backfill_peaks(const DifficultyObject& current, InitialStrain initial_strain) {
    while (current.start_time > current_section_end) {
      save_current_peak(current_section_end - current_section_begin);
      current_section_begin = current_section_end;
      if (!queued_strains.empty()) {
        const auto [strain, start_time] = queued_strains.front();
        queued_strains.erase(queued_strains.begin());
        current_section_end = start_time + max_section_length;
        current_section_peak = initial_strain(current_section_begin, current);
        current_section_peak = net_max(current_section_peak, strain);
      } else {
        current_section_end = current_section_begin + max_section_length;
        current_section_peak = initial_strain(current_section_begin, current);
      }
    }
  }

  void add_in_place(const StrainPeak& peak) {
    int index = binary_search(strain_peaks, peak, compare_peaks);
    if (index < 0)
      index = ~index;
    strain_peaks.insert(strain_peaks.begin() + index, peak);
  }

  void save_current_peak(double section_length) {
    if (final_peak) {
      const auto found = std::find(strain_peaks.begin(), strain_peaks.end(), *final_peak);
      if (found != strain_peaks.end())
        strain_peaks.erase(found);
      final_peak.reset();
    }
    add_in_place(StrainPeak(current_section_peak, section_length));
    // The length as given, though peaks keep it rounded.
    total_length += section_length;
    while (total_length > max_stored_length * max_section_length) {
      total_length -= strain_peaks.back().section_length;
      strain_peaks.pop_back();
    }
  }

  const std::vector<StrainPeak>& current_strain_peaks() {
    if (!final_peak) {
      final_peak = StrainPeak(current_section_peak, current_section_end - current_section_begin);
      add_in_place(*final_peak);
    }
    return strain_peaks;
  }

  double count_top_weighted_strains(double difficulty_value) const {
    if (object_difficulties.empty())
      return 0.0;
    const double consistent_top_strain = difficulty_value * (1 - decay_weight);
    if (consistent_top_strain == 0)
      return static_cast<double>(object_difficulties.size());
    double sum = 0;
    for (const double s : object_difficulties)
      sum += logistic(s / consistent_top_strain, 0.88, 10, 1.1);
    return sum;
  }
};

// ---------------------- osu.Game/Rulesets/Difficulty/Skills/StrainSkill.cs

struct StrainSkill {
  std::vector<double> object_difficulties;
  int                 section_length = 400;
  double              current_section_peak = 0;
  double              current_section_end = 0;
  std::vector<double> strain_peaks;

  template <class StrainAt, class InitialStrain>
  double process(const DifficultyObject& current, StrainAt strain_at,
                 InitialStrain initial_strain) {
    if (current.index == 0)
      current_section_end = std::ceil(current.start_time / section_length) * section_length;
    while (current.start_time > current_section_end) {
      strain_peaks.push_back(current_section_peak);
      current_section_peak = initial_strain(current_section_end, current);
      current_section_end += section_length;
    }
    const double strain = strain_at(current);
    current_section_peak = net_max(strain, current_section_peak);
    return strain;
  }

  std::vector<double> current_strain_peaks() const {
    std::vector<double> peaks = strain_peaks;
    peaks.push_back(current_section_peak);
    return peaks;
  }
};

// --------------------- osu.Game.Rulesets.Osu/Difficulty/Skills/Aim.cs

struct Aim : VariableLengthStrainSkill {
  bool                include_sliders = true;
  const Mods*         mods = nullptr;
  double              current_strain = 0;
  std::vector<double> slider_strains;

  static double strain_decay(double ms) { return detail::pow(0.2, ms / 1000); }

  static double snap_flow_probability(double ratio) {
    constexpr double k = 7.27;
    if (ratio == 0)
      return 0;
    if (std::isnan(ratio))
      return 1;
    return logistic(-k * std::log(ratio));
  }

  double total_value(double snap, double agility, double flow) const {
    constexpr double skill_multiplier_total = 1.12;
    constexpr double combined_snap_norm_exponent = 1.2;
    double       combined_snap = norm(combined_snap_norm_exponent, snap, agility);
    const double p_snap = snap_flow_probability(flow / combined_snap);
    const double p_flow = 1 - p_snap;
    if (mods->touch_device) {
      snap = detail::pow(snap, 0.89);
      combined_snap = norm(combined_snap_norm_exponent, snap, agility);
    }
    if (mods->relax) {
      combined_snap *= 0.75;
      flow *= 0.6;
    }
    const double total = combined_snap * p_snap + flow * p_flow;
    return total * skill_multiplier_total;
  }

  double adjusted_difficulty(const DifficultyObject& current) const {
    constexpr double skill_multiplier_snap = 70.9;
    constexpr double skill_multiplier_agility = 2.35;
    constexpr double skill_multiplier_flow = 242.0;
    const double snap = snap_aim_difficulty(current, include_sliders) * skill_multiplier_snap;
    const double agility = agility_difficulty(current) * skill_multiplier_agility;
    const double flow = flow_aim_difficulty(current, include_sliders) * skill_multiplier_flow;
    double total = total_value(snap, agility, flow);
    if (mods->magnetised != 0)
      total *= 1.0 - mods->magnetised;
    total *= 0.985 + detail::pow(net_max(0, current.overall_difficulty()), 2) / 4000;
    return total;
  }

  void process(const DifficultyObject& current) {
    object_difficulties.push_back(VariableLengthStrainSkill::process(
        current,
        [&](const DifficultyObject& c) {
          if (mods->autopilot)
            return 0.0;
          const double decay = strain_decay(c.adjusted_delta_time);
          current_strain *= decay;
          current_strain += adjusted_difficulty(c) * (1 - decay);
          if (c.base->is_slider())
            slider_strains.push_back(current_strain);
          return current_strain;
        },
        [&](double time, const DifficultyObject& c) {
          return current_strain * strain_decay(time - c.previous()->start_time);
        }));
  }

  double difficult_sliders() const {
    if (slider_strains.empty())
      return 0;
    const double max_strain = *std::max_element(slider_strains.begin(), slider_strains.end());
    if (max_strain == 0)
      return 0;
    double sum = 0;
    for (const double strain : slider_strains)
      sum += logistic(strain / max_strain, 0.5, 12.0);
    return sum;
  }

  double count_top_weighted_sliders(double difficulty_value) const {
    if (slider_strains.empty())
      return 0;
    const double consistent_top_strain = difficulty_value * (1 - decay_weight);
    if (consistent_top_strain == 0)
      return 0;
    double sum = 0;
    for (const double s : slider_strains)
      sum += logistic(s / consistent_top_strain, 0.88, 10, 1.1);
    return sum;
  }

  std::vector<StrainPeak> reduced_strain_peaks() {
    constexpr int    reduced_section_time = 4000;
    constexpr double reduced_strain_baseline = 0.727;
    std::vector<StrainPeak> strains;
    for (const StrainPeak& p : current_strain_peaks())
      if (p.value > 0)
        strains.push_back(p);
    constexpr int chunk_size = 20;
    double        time = 0;
    size_t        skip_count = 0;
    while (strains.size() > skip_count && time < reduced_section_time) {
      const StrainPeak strain = strains[skip_count];
      for (double added_time = 0; added_time < strain.section_length; added_time += chunk_size) {
        const double scale = std::log10(
            lerp(1, 10, net_clamp((time + added_time) / reduced_section_time, 0, 1)));
        strains.emplace_back(strain.value * lerp(reduced_strain_baseline, 1.0, scale),
                             net_min(chunk_size, strain.section_length - added_time));
      }
      time += strain.section_length;
      ++skip_count;
    }
    std::vector<StrainPeak> rest(strains.begin() + static_cast<std::ptrdiff_t>(skip_count),
                                 strains.end());
    std::stable_sort(rest.begin(), rest.end(), [](const StrainPeak& a, const StrainPeak& b) {
      return compare(a.value, b.value) > 0;
    });
    return rest;
  }

  double difficulty_value() {
    double     difficulty = 0;
    double     time = 0;
    const auto strains = reduced_strain_peaks();
    for (const StrainPeak& strain : strains) {
      const double start_time = time;
      const double end_time = time + strain.section_length / max_section_length;
      const double weight =
          detail::pow(decay_weight, start_time) - detail::pow(decay_weight, end_time);
      difficulty += strain.value * weight;
      time = end_time;
    }
    return difficulty / (1 - decay_weight);
  }
};

// ------------------- osu.Game.Rulesets.Osu/Difficulty/Skills/Speed.cs

struct Speed : HarmonicSkill {
  const Mods*         mods = nullptr;
  std::vector<double> slider_strains;
  double              current_strain = 0;

  Speed() {
    harmonic_scale = 20;
    decay_exponent = 0.9;
  }
  static double strain_decay(double ms) { return detail::pow(0.3, ms / 1000); }

  void process(const DifficultyObject& current) {
    object_difficulties.push_back(object_difficulty(current));
  }
  double object_difficulty(const DifficultyObject& current) {
    constexpr double skill_multiplier = 1.16;
    if (mods->relax)
      return 0;
    const double decay = strain_decay(current.adjusted_delta_time);
    current_strain *= decay;
    double difficulty = speed_difficulty(current);
    if (mods->autopilot)
      difficulty *= 0.5;
    current_strain += difficulty * (1 - decay) * skill_multiplier;
    const double rhythm = rhythm_difficulty(current);
    const double total = current_strain * rhythm;
    if (current.base->is_slider())
      slider_strains.push_back(total);
    return total;
  }
  double difficulty_value() { return harmonic_value(object_difficulties); }

  double relevant_object_count() const {
    if (object_difficulties.empty())
      return 0;
    const double max_strain =
        *std::max_element(object_difficulties.begin(), object_difficulties.end());
    if (max_strain == 0)
      return 0;
    double sum = 0;
    for (const double strain : object_difficulties)
      sum += logistic(strain / max_strain, 0.5, 12.0);
    return sum;
  }
  double count_top_weighted_sliders(double difficulty_value) const {
    if (slider_strains.empty() || object_weight_sum == 0)
      return 0;
    const double consistent_top = difficulty_value / object_weight_sum;
    if (consistent_top == 0)
      return 0;
    double sum = 0;
    for (const double s : slider_strains)
      sum += logistic(s / consistent_top, 0.88, 10, 1.1);
    return sum;
  }
};

// ----------------- osu.Game.Rulesets.Osu/Difficulty/Skills/Reading.cs

struct Reading : HarmonicSkill {
  const Mods*           mods = nullptr;
  bool                  has_hidden = false;
  double                current_strain = 0;
  double                reduced_note_count = 0;
  std::optional<double> reduced_duration;

  static double strain_decay(double ms) { return detail::pow(0.8, ms / 1000); }

  void process(const DifficultyObject& current) {
    constexpr double skill_multiplier = 2.5;
    constexpr double reduced_difficulty_duration = 60 * 1000;
    const double     decay = strain_decay(current.delta_time);
    current_strain *= decay;
    current_strain += adjusted_difficulty(current) * (1 - decay) * skill_multiplier;
    if (!reduced_duration)
      reduced_duration = current.start_time + reduced_difficulty_duration;
    if (current.start_time <= *reduced_duration)
      reduced_note_count++;
    object_difficulties.push_back(current_strain);
  }
  double adjusted_difficulty(const DifficultyObject& current) const {
    double difficulty = reading_difficulty(current, has_hidden);
    if (mods->touch_device)
      difficulty = detail::pow(difficulty, 0.89);
    if (mods->magnetised != 0)
      difficulty *= 1.0 - mods->magnetised;
    if (mods->relax)
      difficulty *= 0.4;
    if (mods->autopilot)
      difficulty *= 0.1;
    difficulty *= 0.825 + detail::pow(net_max(0, current.overall_difficulty()), 2.2) / 1125.0;
    return difficulty;
  }
  double difficulty_value() {
    std::vector<double> difficulties;
    for (const double v : object_difficulties)
      if (v > 0)
        difficulties.push_back(v);
    constexpr double reduced_difficulty_base_line = 0.0;
    for (int i = 0; i < net_min(static_cast<double>(difficulties.size()), reduced_note_count); ++i) {
      const double scale =
          std::log10(lerp(1, 10, net_clamp(i / reduced_note_count, 0, 1)));
      difficulties[i] *= lerp(reduced_difficulty_base_line, 1.0, scale);
    }
    return harmonic_value(std::move(difficulties));
  }
};

// -------------- osu.Game.Rulesets.Osu/Difficulty/Skills/Flashlight.cs

struct Flashlight : StrainSkill {
  const Mods* mods = nullptr;
  int         total_objects = 0;
  double      current_strain = 0;

  static double strain_decay(double ms) { return detail::pow(0.15, ms / 1000); }

  void process(const DifficultyObject& current) {
    object_difficulties.push_back(StrainSkill::process(
        current,
        [&](const DifficultyObject& c) {
          constexpr double skill_multiplier = 0.058;
          if (!mods->flashlight)
            return 0.0;
          current_strain *= strain_decay(c.delta_time);
          current_strain += adjusted_difficulty(c) * skill_multiplier;
          return current_strain;
        },
        [&](double time, const DifficultyObject& c) {
          return current_strain * strain_decay(time - c.previous()->start_time);
        }));
  }
  double adjusted_difficulty(const DifficultyObject& current) const {
    double difficulty = flashlight_difficulty(current, *mods);
    if (mods->touch_device)
      difficulty = detail::pow(difficulty, 0.9);
    if (mods->magnetised != 0)
      difficulty *= 1.0 - mods->magnetised;
    if (mods->deflate_start_scale != 0)
      difficulty *= net_clamp(reverse_lerp(mods->deflate_start_scale, 11, 1), 0.1, 1);
    if (mods->relax)
      difficulty *= 0.7;
    if (mods->autopilot)
      difficulty *= 0.4;
    difficulty *= 0.985 + detail::pow(net_max(0, current.overall_difficulty()), 2) / 4000;
    return difficulty;
  }
  double difficulty_value() const {
    double sum = 0;
    for (const double peak : current_strain_peaks())
      sum += peak;
    sum *= 0.7 + 0.1 * net_min(1.0, total_objects / 200.0) +
           (total_objects > 200 ? 0.2 * net_min(1.0, (total_objects - 200) / 200.0) : 0.0);
    return sum;
  }
  static double difficulty_to_performance(double difficulty) {
    return 25 * detail::pow(difficulty, 2);
  }
};

// ------------ osu.Game/Rulesets/Objects/Legacy/LegacyRulesetExtensions.cs

// A float as C#'s (decimal)(double) holds it, to its 15 significant digits,
// in units of 1e-16. Settings are under 100, and none between 0 and 0.01
// keeps a digit it would round off here.
inline long long decimal_e16(float value) {
  if (value == 0)
    return 0;
  char text[32];
  std::snprintf(text, sizeof text, "%.14e", static_cast<double>(value));
  const bool  negative = text[0] == '-';
  long long   digits = 0;
  const char* p = text + negative;
  for (; *p && *p != 'e'; ++p)
    if (*p >= '0' && *p <= '9')
      digits = digits * 10 + (*p - '0');
  // The digits are value * 10^(14 - exponent); a unit is 1e-16.
  for (int shift = std::atoi(p + 1) + 2; shift != 0; shift += shift < 0 ? 1 : -1)
    digits = shift > 0 ? digits * 10 : digits / 10;
  return negative ? -digits : digits;
}

// a / b against c / d, for numerators from 0 and denominators above 0:
// below 0, 0 or above 0 as a / b is less, equal or more. By their continued
// fractions, so nothing overflows.
inline int compare_fractions(unsigned long long a,
                             unsigned long long b,
                             unsigned long long c,
                             unsigned long long d) {
  for (int sign = 1;; sign = -sign) {
    if (a / b != c / d)
      return a / b < c / d ? -sign : sign;
    a %= b;
    c %= d;
    if (a == 0 || c == 0)
      return a == c ? 0 : a == 0 ? -sign : sign;
    // Below 1 both: a / b is less exactly when b / a is more.
    std::swap(a, b);
    std::swap(c, d);
  }
}

// osu!stable's score multiplier, its "difficulty peppy stars": the map's
// own HP, OD and CS, and its objects per second of drain times 8, up to 16,
// together over 38, times 5, rounded half to even. osu! works this out in
// C#'s decimal, as osu!stable's 80-bit floats come closest to; it's exact
// here, which rounds the same: a value off a half by decimal's rounding is
// off it by far more exactly.
inline int difficulty_peppy_stars(float drain_rate,
                                  float overall_difficulty,
                                  float circle_size,
                                  int   object_count,
                                  int   drain_length) {
  const long long settings = decimal_e16(drain_rate) +
                             decimal_e16(overall_difficulty) +
                             decimal_e16(circle_size);
  // The objects per second of drain times 8, clamped, as ratio / per.
  long long ratio = 16, per = 1;
  if (drain_length != 0) {
    ratio = 8LL * object_count;
    per = drain_length;
    if (per < 0) {
      ratio = -ratio;
      per = -per;
    }
    if (ratio < 0)
      ratio = 0;
    if (ratio > 16 * per) {
      ratio = 16;
      per = 1;
    }
  }
  // The stars are the halves, n + 0.5, the value is above, where the
  // settings and the ratio together are 3.8 (2n + 1); on a half, the even
  // of n and n + 1.
  for (int stars = 0;; ++stars) {
    // 3.8 (2n + 1) less the settings, in units of 1e-16, for the ratio.
    const long long half =
        38LL * (2 * stars + 1) * 1'000'000'000'000'000 - settings;
    const int above =
        half < 0 ? 1
                 : compare_fractions(ratio, per, half,
                                     10'000'000'000'000'000);  // 1e16
    if (above < 0)
      return stars;
    if (above == 0)
      return stars + stars % 2;
  }
}

// --------------- osu.Game.Rulesets.Osu/Difficulty/Utils/LegacyScoreUtils.cs

// The map's drain time in whole seconds, as osu!stable works it out: from
// its first object's start to its last's, less its breaks.
inline int drain_length(const Beatmap& map) {
  if (map.objects.empty())
    return 0;
  int breaks = 0;
  for (const Break& b : map.breaks)
    breaks += static_cast<int>(round_half_even(b.end_time)) -
              static_cast<int>(round_half_even(b.start_time));
  return (static_cast<int>(round_half_even(map.objects.back().start_time)) -
          static_cast<int>(round_half_even(map.objects.front().start_time)) -
          breaks) /
         1000;
}

// What osu!stable's score gives a spinner's spins: every full spin's, and
// the bonus spins' after it's completed, as osu!stable needs it spun at
// least 3 times a second and allows at most 477 a minute, halving the bonus
// for an average case.
inline double spinner_score(const HitObject& spinner) {
  constexpr double maximum_rotations_per_second = 477.0 / 60;
  constexpr double minimum_rotations_per_second = 3;
  const double     seconds = (spinner.end_time - spinner.start_time) / 1000;
  const int total_half_spins =
      static_cast<int>(seconds * maximum_rotations_per_second * 2);
  const int half_spins_to_complete =
      static_cast<int>(seconds * minimum_rotations_per_second);
  const int half_spins_before_bonus = half_spins_to_complete + 3;
  long long score = 0;
  const int full_spins = total_half_spins / 2;
  score += 100LL * full_spins;
  int bonus_spins = (total_half_spins - half_spins_before_bonus) / 2;
  bonus_spins = std::max(0, bonus_spins - full_spins / 2);
  score += 1000LL * bonus_spins;
  return static_cast<double>(score);
}

// The score osu!stable gives the first `count` objects' slider parts and
// spinner spins, per object.
inline double nested_score_per_object(const std::vector<HitObject>& objects,
                                      size_t                        count) {
  int    big_ticks = 0, small_ticks = 0;
  double spinners = 0;
  for (size_t i = 0; i < count; ++i) {
    const HitObject& h = objects[i];
    if (h.kind == HitObject::slider) {
      // Its head, its tail and its repeats.
      big_ticks += 2 + h.repeat_count;
      for (const NestedObject& n : h.nested)
        small_ticks += n.kind == NestedObject::tick;
    } else if (h.kind == HitObject::spinner) {
      spinners += spinner_score(h);
    }
  }
  const double sliders = big_ticks * 30.0 + small_ticks * 10.0;
  return (sliders + spinners) / static_cast<int>(count);
}

// ------------ osu.Game.Rulesets.Osu/Difficulty/OsuLegacyScoreSimulator.cs

// A full combo of the first `count` objects as osu!stable scores it: its
// combo, and the bonus its combo adds to each object's score with the map's
// score multiplier.
struct LegacyCombo {
  long long score = 0;
  int       combo = 0;
};

inline LegacyCombo legacy_combo(const std::vector<HitObject>& objects,
                                size_t                        count,
                                double                        multiplier) {
  LegacyCombo c;
  const auto  hit = [&](int score, bool adds_combo, bool combo_bonus) {
    if (combo_bonus)
      c.score += static_cast<int>(std::max(0, c.combo - 1) *
                                  (score / 25 * multiplier));
    if (adds_combo)
      ++c.combo;
  };
  for (size_t i = 0; i < count; ++i) {
    const HitObject& h = objects[i];
    if (h.kind == HitObject::slider) {
      for (const NestedObject& n : h.nested)
        hit(n.kind == NestedObject::tick ? 10 : 30, true, false);
      hit(300, false, true);
    } else {
      // A circle, or a spinner: its spins are bonus, outside the combo.
      hit(300, true, true);
    }
  }
  return c;
}

// ---------- osu.Game.Rulesets.Osu/Difficulty/OsuDifficultyCalculator.cs

constexpr double performance_base_multiplier = 1.12;  // OsuPerformanceCalculator
constexpr double performance_norm_exponent = 1.1;

inline double aim_difficulty_to_performance(double difficulty) {
  return 4.0 * detail::pow(difficulty, 3);
}
inline double sum_cognition_difficulty(double reading, double flashlight) {
  if (reading <= 0)
    return flashlight;
  if (flashlight <= 0)
    return reading;
  return norm(performance_norm_exponent, reading,
              flashlight * net_clamp(flashlight / reading, 0.25, 1.0));
}
inline double aim_difficulty_rating(double value) { return detail::pow(value, 0.63) * 0.02275; }
inline double difficulty_rating(double value) { return std::sqrt(value) * 0.0675; }

// A calculation's state: the map's objects and its skills as they've been
// given objects so far.
struct Calculation {
  const Beatmap*                map;
  const Mods*                   mods;
  std::vector<Object>           objects;
  std::vector<DifficultyObject> difficulty_objects;
  std::vector<size_t>           order;  // difficulty objects by start time
  Aim                           aim, aim_no_sliders;
  Speed                         speed;
  Reading                       reading;
  std::optional<Flashlight>     flashlight;

  Calculation(const Beatmap& map, const Mods& m) : map(&map), mods(&m) {
    objects.reserve(map.objects.size());
    for (const HitObject& h : map.objects)
      objects.push_back(make_object(h, map, m));
    difficulty_objects.reserve(objects.size());
    for (size_t i = 1; i < objects.size(); ++i)
      difficulty_objects.push_back(make_difficulty_object(
          objects[i], objects[i - 1], m.clock_rate, difficulty_objects,
          static_cast<int>(difficulty_objects.size())));
    order.resize(difficulty_objects.size());
    for (size_t i = 0; i < order.size(); ++i)
      order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
      return compare(difficulty_objects[a].base->start_time,
                     difficulty_objects[b].base->start_time) < 0;
    });
    aim.mods = aim_no_sliders.mods = speed.mods = reading.mods = &m;
    aim_no_sliders.include_sliders = false;
    reading.has_hidden = m.hidden && !m.hidden_only_fades_approach_circles;
    if (m.flashlight) {
      flashlight.emplace();
      flashlight->mods = &m;
      flashlight->total_objects = static_cast<int>(map.objects.size());
    }
  }

  void process(const DifficultyObject& d) {
    aim.process(d);
    aim_no_sliders.process(d);
    speed.process(d);
    reading.process(d);
    if (flashlight)
      flashlight->process(d);
  }

  // The attributes from the objects processed so far, of `counted` objects
  // of the map.
  Attributes attributes(size_t counted) {
    Attributes a;
    if (counted == 0)
      return a;
    for (size_t i = 0; i < counted; ++i) {
      const HitObject::Kind kind = map->objects[i].kind;
      a.hit_circle_count += kind == HitObject::circle;
      a.slider_count += kind == HitObject::slider;
      a.spinner_count += kind == HitObject::spinner;
    }
    a.nested_score_per_object = nested_score_per_object(map->objects, counted);
    const int multiplier = difficulty_peppy_stars(
        map->own_drain_rate, map->own_overall_difficulty, map->own_circle_size,
        static_cast<int>(map->objects.size()), drain_length(*map));
    a.legacy_score_base_multiplier = multiplier;
    const LegacyCombo combo = legacy_combo(map->objects, counted, multiplier);
    a.max_combo = combo.combo;
    a.maximum_legacy_combo_score = static_cast<double>(combo.score);
    const double aim_value = aim.difficulty_value();
    const double aim_no_sliders_value = aim_no_sliders.difficulty_value();
    const double speed_value = speed.difficulty_value();
    const double reading_value = reading.difficulty_value();
    const double aim_difficult_strain_count = aim.count_top_weighted_strains(aim_value);
    const double speed_difficult_strain_count =
        speed.count_top_weighted(speed_value, 0.88, 10);
    const double reading_difficult_note_count =
        reading.count_top_weighted(reading_value, 1.15, 5);
    const double speed_notes = speed.relevant_object_count();
    const double aim_no_sliders_top_weighted_slider_count =
        aim_no_sliders.count_top_weighted_sliders(aim_no_sliders_value);
    const double aim_no_sliders_difficult_strain_count =
        aim_no_sliders.count_top_weighted_strains(aim_no_sliders_value);
    const double aim_top_weighted_slider_factor =
        aim_no_sliders_top_weighted_slider_count /
        net_max(1, aim_no_sliders_difficult_strain_count - aim_no_sliders_top_weighted_slider_count);
    const double speed_top_weighted_slider_count = speed.count_top_weighted_sliders(speed_value);
    const double speed_top_weighted_slider_factor =
        speed_top_weighted_slider_count /
        net_max(1, speed_difficult_strain_count - speed_top_weighted_slider_count);
    const double difficult_sliders = aim.difficult_sliders();
    const double aim_rating = aim_difficulty_rating(aim_value);
    const double aim_no_sliders_rating = aim_difficulty_rating(aim_no_sliders_value);
    const double slider_factor = aim_value > 0 ? aim_no_sliders_rating / aim_rating : 1;
    const double speed_rating = difficulty_rating(speed_value);
    const double reading_rating = difficulty_rating(reading_value);
    std::optional<double> flashlight_rating;
    if (flashlight)
      flashlight_rating = difficulty_rating(flashlight->difficulty_value());
    a.star_rating = osu_pp::star_rating(aim_rating, speed_rating, reading_rating,
                                        flashlight_rating);
    a.aim = aim_rating;
    a.aim_difficult_slider_count = difficult_sliders;
    a.speed = speed_rating;
    a.speed_note_count = speed_notes;
    a.flashlight = flashlight_rating;
    a.reading = reading_rating;
    a.slider_factor = slider_factor;
    a.aim_difficult_strain_count = aim_difficult_strain_count;
    a.speed_difficult_strain_count = speed_difficult_strain_count;
    a.reading_difficult_note_count = reading_difficult_note_count;
    a.aim_top_weighted_slider_factor = aim_top_weighted_slider_factor;
    a.speed_top_weighted_slider_factor = speed_top_weighted_slider_factor;
    return a;
  }
};

}  // namespace detail

inline double aim_rating(double difficulty_value) {
  return detail::aim_difficulty_rating(difficulty_value);
}
inline double rating(double difficulty_value) {
  return detail::difficulty_rating(difficulty_value);
}
inline double star_rating(double aim, double speed, double reading,
                          std::optional<double> flashlight) {
  using namespace detail;
  const double base_aim = aim_difficulty_to_performance(aim);
  const double base_speed = harmonic_difficulty_to_performance(speed);
  const double base_reading = harmonic_difficulty_to_performance(reading);
  const double base_flashlight =
      flashlight ? Flashlight::difficulty_to_performance(*flashlight) : 0.0;
  const double base_cognition = sum_cognition_difficulty(base_reading, base_flashlight);
  const double base_performance =
      norm(performance_norm_exponent, base_aim, base_speed, base_cognition);
  return std::cbrt(base_performance * performance_base_multiplier);
}

inline Difficulty calculate(const Beatmap& map, const Mods& mods) {
  detail::Calculation c(map, mods);
  for (const size_t i : c.order)
    c.process(c.difficulty_objects[i]);
  Difficulty result;
  result.attributes = c.attributes(c.objects.size());
  result.objects.aim = c.aim.object_difficulties;
  result.objects.aim_no_sliders = c.aim_no_sliders.object_difficulties;
  result.objects.speed = c.speed.object_difficulties;
  result.objects.reading = c.reading.object_difficulties;
  if (c.flashlight)
    result.objects.flashlight = c.flashlight->object_difficulties;
  // Rating the map summed speed's and reading's weights.
  result.objects.aim_weight = 1 / (1 - c.aim.decay_weight);
  result.objects.speed_weight = c.speed.object_weight_sum;
  result.objects.reading_weight = c.reading.object_weight_sum;
  return result;
}

inline std::vector<TimedAttributes> calculate_timed(const Beatmap& map, const Mods& mods) {
  detail::Calculation          c(map, mods);
  std::vector<TimedAttributes> steps;
  size_t                       next = 0;
  for (const detail::Object& o : c.objects) {
    while (next < c.order.size() &&
           c.difficulty_objects[c.order[next]].base->end_time <= o.end_time)
      c.process(c.difficulty_objects[c.order[next++]]);
    steps.push_back({o.end_time, c.attributes(steps.size() + 1)});
  }
  return steps;
}

}  // namespace osu_pp
