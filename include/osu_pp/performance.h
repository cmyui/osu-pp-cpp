// osu-pp-cpp: osu!'s performance (pp) calculation for osu!standard, ported
// to C++ from ppy/osu at 2026.1026.0, as difficulty.h's star rating is (see
// README.md): each section is one of osu!'s source files, with its
// arithmetic, so the results match osu!'s bit for bit.
//
// A score's pp is from its map's difficulty attributes (calculate's), the
// mods it was set with, and what it holds (Score). osu! reads a score three
// ways, by what it can know of its slider breaks:
//
// - set on osu!stable: Classic's slider accuracy and its osu!stable total
//   score, from which osu! estimates its misses and slider breaks;
// - set on osu!lazer with Classic: Classic's slider accuracy, and its
//   misses and slider breaks estimated from its max combo;
// - set on osu!lazer: its slider tails and ticks as judged.
//
// Licensed under the MIT licence (LICENSE), as osu! is.
#pragma once

#include <osu_pp/difficulty.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>

namespace osu_pp {

// What a score holds that the calculation reads.
struct Score {
  // osu!'s accuracy for the score, 0 to 1: osu!stable's, from its 300s,
  // 100s, 50s and misses, for a score set there.
  double accuracy = 1;
  int    max_combo = 0;
  int    great = 0;  // 300s
  int    ok = 0;     // 100s
  int    meh = 0;    // 50s
  int    miss = 0;
  // Judged on osu!lazer without Classic's slider accuracy only: its slider
  // tails hit, and its slider ticks and repeats missed.
  int    slider_tail_hits = 0;
  int    large_tick_misses = 0;
  // Its osu!stable total score, for a score set there.
  std::optional<long long> legacy_total_score;
};

// osu!'s OsuPerformanceAttributes: each part's pp, the total, and what the
// calculation estimated on the way.
struct Performance {
  double                aim = 0;
  double                speed = 0;
  double                accuracy = 0;
  double                flashlight = 0;
  double                reading = 0;
  double                effective_miss_count = 0;
  double                combo_based_estimated_miss_count = 0;
  std::optional<double> score_based_estimated_miss_count;
  double                aim_estimated_slider_breaks = 0;
  double                speed_estimated_slider_breaks = 0;
  std::optional<double> speed_deviation;
  double                total = 0;
};

// The pp of `score`, set with `mods` on `map`, whose attributes are
// `attributes` (calculate's with the same mods).
inline Performance performance(const Attributes& attributes,
                               const Beatmap&    map,
                               const Mods&       mods,
                               const Score&      score);

namespace detail {

// --------------- osu.Game/Rulesets/Difficulty/Utils/DifficultyCalculationUtils

constexpr double sqrt2 = 1.4142135623730950;

// The Abramowitz and Stegun approximation (7.1.26) osu! uses.
inline double erf(double x) {
  if (x == 0)
    return 0;
  if (std::isinf(x))
    return x > 0 ? 1 : -1;
  if (std::isnan(x))
    return x;
  const double t = 1.0 / (1.0 + 0.3275911 * std::abs(x));
  const double tau =
      t * (0.254829592 +
           t * (-0.284496736 +
                t * (1.421413741 + t * (-1.453152027 + t * 1.061405429))));
  const double result = 1.0 - tau * std::exp(-x * x);
  return x >= 0 ? result : -result;
}

inline int net_sign(double x) { return x > 0 ? 1 : x < 0 ? -1 : 0; }

// Winitzki's approximation, corrected near 1, as osu! has it.
inline double erf_inv(double x) {
  if (x <= -1)
    return -std::numeric_limits<double>::infinity();
  if (x >= 1)
    return std::numeric_limits<double>::infinity();
  if (x == 0)
    return 0;
  constexpr double a = 0.147;
  const double     sign = net_sign(x);
  x = std::abs(x);
  const double ln = std::log(1 - x * x);
  const double t1 = 2 / (std::numbers::pi * a) + ln / 2;
  const double t2 = ln / a;
  const double base = std::sqrt(t1 * t1 - t2) - t1;
  const double correction = x >= 0.85 ? pow((x - 0.85) / 0.293, 8) : 0;
  return sign * (std::sqrt(base) + correction);
}

// ------------------------------------ osu.Game/Beatmaps/IBeatmapDifficultyInfo

inline double inverse_difficulty_range(double value,
                                       double diff0,
                                       double diff5,
                                       double diff10) {
  return net_sign(value - diff5) == net_sign(diff10 - diff5)
             ? (value - diff5) / (diff10 - diff5) * 5 + 5
             : (value - diff5) / (diff5 - diff0) * 5 + 5;
}

// ---------------------------- osu.Game.Rulesets.Osu/Scoring/OsuHitWindows.cs

// The window a judgement needs, either side, ms: as osu!stable floors it,
// less a half for its "under" rather than "at most".
inline double hit_window(double overall_difficulty,
                         double od0,
                         double od5,
                         double od10) {
  return std::floor(difficulty_range(overall_difficulty, od0, od5, od10)) - 0.5;
}

// ------------ osu.Game.Rulesets.Osu/Difficulty/OsuLegacyScoreMissCalculator.cs

// osu!stable's score multiplier for the mods: none for Relax and
// Autopilot, which score nothing there.
inline double legacy_score_multiplier(const Mods& mods) {
  double multiplier = 1.0;
  if (mods.no_fail)
    multiplier *= mods.score_v2 ? 1.0 : 0.5;
  if (mods.easy)
    multiplier *= 0.5;
  if (mods.half_time)
    multiplier *= 0.3;
  if (mods.hidden)
    multiplier *= 1.06;
  if (mods.hard_rock)
    multiplier *= mods.score_v2 ? 1.10 : 1.06;
  if (mods.double_time)
    multiplier *= mods.score_v2 ? 1.20 : 1.12;
  if (mods.flashlight)
    multiplier *= 1.12;
  if (mods.spun_out)
    multiplier *= 0.9;
  if (mods.relax || mods.autopilot)
    return 0;
  return multiplier;
}

// The misses and slider breaks an osu!stable score's total implies: what
// its total falls short of a full combo's by, against what a miss would
// cost, capped by a harsh reading of its max combo.
struct LegacyScoreMissCalculator {
  const Score&      score;
  const Attributes& attributes;
  const Mods&       mods;

  double calculate() const {
    if (attributes.max_combo == 0 || !score.legacy_total_score ||
        *score.legacy_total_score == 0)
      return 0;
    const double score_v1_multiplier =
        attributes.legacy_score_base_multiplier * legacy_score_multiplier(mods);
    const double relevant_combo_per_object = relevant_score_combo_per_object();
    const double maximum_miss_count = maximum_combo_based_miss_count();
    const double score_during_max_combo = score_at_combo(
        score.max_combo, relevant_combo_per_object, score_v1_multiplier);
    const double remaining_score =
        static_cast<double>(*score.legacy_total_score) - score_during_max_combo;
    if (remaining_score <= 0)
      return maximum_miss_count;
    const double remaining_combo = attributes.max_combo - score.max_combo;
    const double expected_remaining_score = score_at_combo(
        remaining_combo, relevant_combo_per_object, score_v1_multiplier);
    double score_based_miss_count = expected_remaining_score / remaining_score;
    // Under a miss is left to the combo-based count, to say if it's a full
    // combo.
    score_based_miss_count = net_max(score_based_miss_count, 1);
    return net_min(score_based_miss_count, maximum_miss_count);
  }

  // The score a combo makes: its combo portion an arithmetic progression of
  // the combo per object, and the rest each object's own.
  double score_at_combo(double combo,
                        double relevant_combo_per_object,
                        double score_v1_multiplier) const {
    const int    total_hits = score.great + score.ok + score.meh + score.miss;
    const double estimated_objects = combo / relevant_combo_per_object - 1;
    double       combo_score =
        relevant_combo_per_object > 0
                  ? (2 * (relevant_combo_per_object - 1) +
                     (estimated_objects - 1) * relevant_combo_per_object) *
                        estimated_objects / 2
                  : 0;
    combo_score *= score.accuracy * 300 / 25 * score_v1_multiplier;
    const double objects_hit =
        (total_hits - score.miss) * combo / attributes.max_combo;
    const double non_combo_score = (300 + attributes.nested_score_per_object) *
                                   score.accuracy * objects_hit;
    return combo_score + non_combo_score;
  }

  // The combo per object a full combo's combo bonus implies, for maps of
  // buzz sliders and the like that don't fit the progression.
  double relevant_score_combo_per_object() const {
    double combo_score = attributes.maximum_legacy_combo_score;
    combo_score /= 300.0 / 25.0 * attributes.legacy_score_base_multiplier;
    // In C#'s int arithmetic, which wraps over 2^31.
    double result = static_cast<double>(static_cast<std::int32_t>(
        static_cast<std::uint32_t>(attributes.max_combo - 2) *
        static_cast<std::uint32_t>(attributes.max_combo)));
    result /= net_max(attributes.max_combo + 2 * (combo_score - 1), 1);
    return result;
  }

  double maximum_combo_based_miss_count() const {
    if (attributes.slider_count <= 0)
      return score.miss;
    const int total_imperfect_hits = score.ok + score.meh + score.miss;
    double    miss_count = 0;
    const double likely_missed_sliderend_portion =
        0.04 +
        0.06 * pow(net_min(attributes.aim_top_weighted_slider_factor, 1), 2);
    const double full_combo_threshold =
        attributes.max_combo -
        net_min(4 + likely_missed_sliderend_portion * attributes.slider_count,
                attributes.slider_count);
    if (score.max_combo < full_combo_threshold)
      miss_count = pow(full_combo_threshold / net_max(1.0, score.max_combo), 2.5);
    miss_count = net_min(miss_count, total_imperfect_hits);
    const int max_possible_slider_breaks = std::min(
        attributes.slider_count, (attributes.max_combo - score.max_combo) / 2);
    const double slider_breaks = miss_count - score.miss;
    if (slider_breaks > max_possible_slider_breaks)
      miss_count = score.miss + max_possible_slider_breaks;
    return miss_count;
  }
};

// --------------- osu.Game.Rulesets.Osu/Difficulty/OsuPerformanceCalculator.cs

struct PerformanceCalculator {
  const Attributes& attributes;
  const Beatmap&    map;
  const Mods&       mods;
  const Score&      score;

  bool   using_classic_slider_accuracy = false;
  bool   using_score_v2 = false;
  double accuracy = 0;
  int    score_max_combo = 0;
  int    count_great = 0, count_ok = 0, count_meh = 0, count_miss = 0;
  int    count_slider_tick_miss = 0;
  int    count_slider_ends_dropped = 0;
  double effective_miss_count = 0;
  double clock_rate = 1;
  double great_hit_window = 0, ok_hit_window = 0, meh_hit_window = 0;
  double overall_difficulty = 0, approach_rate = 0, drain_rate = 0;
  std::optional<double> speed_deviation = std::nullopt;
  double aim_estimated_slider_breaks = 0;
  double speed_estimated_slider_breaks = 0;

  int total_hits() const {
    return count_great + count_ok + count_meh + count_miss;
  }
  int total_successful_hits() const {
    return count_great + count_ok + count_meh;
  }
  int total_imperfect_hits() const { return count_ok + count_meh + count_miss; }

  Performance calculate() {
    using_classic_slider_accuracy = mods.classic_slider_accuracy;
    using_score_v2 = mods.score_v2;
    accuracy = net_clamp(score.accuracy, 0, 1);
    score_max_combo = std::clamp(score.max_combo, 0, attributes.max_combo);
    count_great = score.great;
    count_ok = score.ok;
    count_meh = score.meh;
    count_miss = score.miss;
    count_slider_ends_dropped = attributes.slider_count - score.slider_tail_hits;
    count_slider_tick_miss = score.large_tick_misses;
    effective_miss_count = count_miss;

    clock_rate = mods.clock_rate;
    great_hit_window = hit_window(map.overall_difficulty, 80, 50, 20) / clock_rate;
    ok_hit_window = hit_window(map.overall_difficulty, 140, 100, 60) / clock_rate;
    meh_hit_window = hit_window(map.overall_difficulty, 200, 150, 100) / clock_rate;
    approach_rate = rate_adjusted_approach_rate(map.approach_rate, clock_rate);
    overall_difficulty = (79.5 - great_hit_window) / 6;
    drain_rate = map.drain_rate;

    Performance p;
    p.combo_based_estimated_miss_count = combo_based_estimated_miss_count();
    if (using_classic_slider_accuracy && !using_score_v2 &&
        score.legacy_total_score.value_or(0) > 0) {
      p.score_based_estimated_miss_count =
          LegacyScoreMissCalculator{score, attributes, mods}.calculate();
      effective_miss_count = *p.score_based_estimated_miss_count;
    } else {
      effective_miss_count = p.combo_based_estimated_miss_count;
    }
    effective_miss_count = net_max(count_miss, effective_miss_count);
    effective_miss_count = net_min(total_hits(), effective_miss_count);
    effective_miss_count = net_max(0, effective_miss_count);

    if (effective_miss_count > 0) {
      aim_estimated_slider_breaks =
          estimated_slider_breaks(attributes.aim_top_weighted_slider_factor);
      speed_estimated_slider_breaks =
          estimated_slider_breaks(attributes.speed_top_weighted_slider_factor);
    }

    double multiplier = performance_base_multiplier;
    if (mods.no_fail)
      multiplier *= net_max(0.90, 1.0 - 0.02 * effective_miss_count);
    if (mods.spun_out && total_hits() > 0)
      multiplier *= 1.0 - pow(static_cast<double>(attributes.spinner_count) /
                                  total_hits(),
                              0.85);
    if (mods.relax) {
      // OD 13.33 is where the 300s' window closes.
      const double ok_multiplier =
          0.75 * net_max(0.0, overall_difficulty > 0.0
                                  ? 1 - overall_difficulty / 13.33
                                  : 1.0);
      const double meh_multiplier =
          net_max(0.0, overall_difficulty > 0.0
                           ? 1 - pow(overall_difficulty / 13.33, 5)
                           : 1.0);
      effective_miss_count =
          net_min(effective_miss_count + count_ok * ok_multiplier +
                      count_meh * meh_multiplier,
                  total_hits());
    }

    speed_deviation = calculate_speed_deviation();
    const double aim_value = compute_aim_value();
    const double speed_value = compute_speed_value();
    const double accuracy_value = compute_accuracy_value();
    const double reading_value = compute_reading_value();
    const double flashlight_value = compute_flashlight_value();
    const double cognition_value =
        sum_cognition_difficulty(reading_value, flashlight_value);
    const double total = norm(performance_norm_exponent, aim_value, speed_value,
                              accuracy_value, cognition_value) *
                         multiplier;

    p.aim = aim_value;
    p.speed = speed_value;
    p.accuracy = accuracy_value;
    p.flashlight = flashlight_value;
    p.reading = reading_value;
    p.effective_miss_count = effective_miss_count;
    p.aim_estimated_slider_breaks = aim_estimated_slider_breaks;
    p.speed_estimated_slider_breaks = speed_estimated_slider_breaks;
    p.speed_deviation = speed_deviation;
    p.total = total;
    return p;
  }

  double compute_aim_value() const {
    if (mods.autopilot)
      return 0.0;
    double aim_difficulty = attributes.aim;
    if (attributes.slider_count > 0 && attributes.aim_difficult_slider_count > 0) {
      double estimate_improperly_followed_difficult_sliders;
      if (using_classic_slider_accuracy) {
        // Classic: all the combo missed is taken for dropped difficult
        // sliders.
        const int maximum_possible_dropped_sliders = total_imperfect_hits();
        estimate_improperly_followed_difficult_sliders = net_clamp(
            std::min(maximum_possible_dropped_sliders,
                     attributes.max_combo - score_max_combo),
            0, attributes.aim_difficult_slider_count);
      } else {
        // Tick misses too mean a slider wasn't followed; misses, of heads,
        // are penalised enough on their own.
        estimate_improperly_followed_difficult_sliders =
            net_clamp(count_slider_ends_dropped + count_slider_tick_miss, 0,
                      attributes.aim_difficult_slider_count);
      }
      const double slider_nerf_factor =
          (1 - attributes.slider_factor) *
              pow(1 - estimate_improperly_followed_difficult_sliders /
                          attributes.aim_difficult_slider_count,
                  3) +
          attributes.slider_factor;
      aim_difficulty *= slider_nerf_factor;
    }
    double aim_value = aim_difficulty_to_performance(aim_difficulty);
    const double length_bonus =
        0.95 + 0.35 * net_min(1.0, total_hits() / 2000.0) +
        (total_hits() > 2000 ? std::log10(total_hits() / 2000.0) * 0.5 : 0.0);
    aim_value *= length_bonus;
    if (effective_miss_count > 0) {
      const double relevant_miss_count =
          net_min(effective_miss_count + aim_estimated_slider_breaks,
                  total_imperfect_hits() + count_slider_tick_miss);
      aim_value *=
          miss_penalty(relevant_miss_count, attributes.aim_difficult_strain_count);
    }
    // Traceable's bonus isn't with Blinds, which hides notes anyway.
    if (mods.blinds)
      aim_value *= 1.3 + (total_hits() * (0.0016 / (1 + 2 * effective_miss_count)) *
                          pow(accuracy, 16)) *
                             (1 - 0.003 * drain_rate * drain_rate);
    else if (mods.traceable)
      aim_value *= 1.0 + traceable_bonus(attributes.slider_factor);
    aim_value *= accuracy;
    return aim_value;
  }

  double compute_speed_value() const {
    if (mods.relax || !speed_deviation)
      return 0.0;
    double speed_value = harmonic_difficulty_to_performance(attributes.speed);
    if (effective_miss_count > 0) {
      const double relevant_miss_count =
          net_min(effective_miss_count + speed_estimated_slider_breaks,
                  total_imperfect_hits() + count_slider_tick_miss);
      speed_value *= miss_penalty(relevant_miss_count,
                                  attributes.speed_difficult_strain_count);
    }
    // Blinds gets the least buff, as for its object count.
    if (mods.blinds)
      speed_value *= 1.12;
    speed_value *= speed_high_deviation_nerf();
    // The hit window speed's stars imply: 20 ms (OD 10) at 4 stars.
    const double effective_hit_window =
        20 * pow(4 / attributes.speed, 0.35);
    // The 300s' share on speed notes in that window.
    const double effective_accuracy =
        erf(effective_hit_window / *speed_deviation);
    speed_value *= pow(effective_accuracy, 2);
    return speed_value;
  }

  double compute_accuracy_value() const {
    if (mods.relax)
      return 0.0;
    // Of objects judged by their timing: circles, and sliders unless
    // Classic's slider accuracy.
    double better_accuracy_percentage;
    int    amount_hit_objects_with_accuracy = attributes.hit_circle_count;
    if (!using_classic_slider_accuracy || using_score_v2)
      amount_hit_objects_with_accuracy += attributes.slider_count;
    if (amount_hit_objects_with_accuracy > 0)
      better_accuracy_percentage =
          ((count_great -
            std::max(total_hits() - amount_hit_objects_with_accuracy, 0)) *
               6 +
           count_ok * 2 + count_meh) /
          static_cast<double>(amount_hit_objects_with_accuracy * 6);
    else
      better_accuracy_percentage = 0;
    if (better_accuracy_percentage < 0)
      better_accuracy_percentage = 0;
    double accuracy_value = pow(1.52163, overall_difficulty) *
                            pow(better_accuracy_percentage, 24) * 2.83;
    // Accuracy held over more circles is worth more.
    accuracy_value *= amount_hit_objects_with_accuracy < 1000
                          ? pow(amount_hit_objects_with_accuracy / 1000.0, 0.3)
                          : pow(amount_hit_objects_with_accuracy / 1000.0, 0.1);
    if (mods.blinds)
      accuracy_value *= 1.14;
    else if (mods.traceable)
      accuracy_value *= 1 + 0.08 * reverse_lerp(approach_rate, 11.5, 10);
    return accuracy_value;
  }

  double compute_flashlight_value() const {
    if (!mods.flashlight)
      return 0.0;
    double flashlight_value =
        Flashlight::difficulty_to_performance(attributes.flashlight.value_or(0));
    // Misses against the objects: 3% off for any.
    if (effective_miss_count > 0)
      flashlight_value *=
          0.97 * pow(1 - pow(effective_miss_count / total_hits(), 0.775),
                     pow(effective_miss_count, .875));
    flashlight_value *= combo_scaling_factor();
    flashlight_value *= 0.5 + accuracy / 2.0;
    return flashlight_value;
  }

  double compute_reading_value() const {
    double reading_value = harmonic_difficulty_to_performance(attributes.reading);
    if (effective_miss_count > 0)
      reading_value *= miss_penalty(effective_miss_count + aim_estimated_slider_breaks,
                                    attributes.reading_difficult_note_count);
    reading_value *= pow(accuracy, 3);
    return reading_value;
  }

  double combo_based_estimated_miss_count() const {
    if (attributes.slider_count <= 0)
      return count_miss;
    double miss_count = count_miss;
    if (using_classic_slider_accuracy) {
      // Hard sliders' ends are likelier dropped; easy sliders likelier broken.
      const double likely_missed_sliderend_portion =
          0.04 + 0.06 * pow(net_min(attributes.aim_top_weighted_slider_factor, 1), 2);
      // A full combo is the max less the slider ends dropped, which Classic
      // doesn't record, so are estimated.
      const double full_combo_threshold =
          attributes.max_combo -
          net_min(4 + likely_missed_sliderend_portion * attributes.slider_count,
                  attributes.slider_count);
      if (score_max_combo < full_combo_threshold)
        miss_count = full_combo_threshold / net_max(1.0, score_max_combo);
      miss_count = net_min(miss_count, total_imperfect_hits());
      // A slider is at least 2 combo, so a break loses at least 2.
      const int max_possible_slider_breaks = std::min(
          attributes.slider_count, (attributes.max_combo - score_max_combo) / 2);
      const double slider_breaks = miss_count - count_miss;
      if (slider_breaks > max_possible_slider_breaks)
        miss_count = count_miss + max_possible_slider_breaks;
    } else {
      const double full_combo_threshold =
          attributes.max_combo - count_slider_ends_dropped;
      if (score_max_combo < full_combo_threshold)
        miss_count = full_combo_threshold / net_max(1.0, score_max_combo);
      // Tick misses break combo too.
      miss_count = net_min(miss_count, count_slider_tick_miss + count_miss);
    }
    return miss_count;
  }

  double estimated_slider_breaks(double top_weighted_slider_factor) const {
    const int non_miss_mistakes = count_ok + count_meh;
    if (!using_classic_slider_accuracy || non_miss_mistakes == 0)
      return 0;
    const double missed_combo_percent =
        1.0 - static_cast<double>(score_max_combo) / attributes.max_combo;
    double estimated = net_min(non_miss_mistakes,
                               effective_miss_count * top_weighted_slider_factor);
    // More 100s and 50s make slider breaks likelier.
    const double non_miss_mistake_adjustment =
        (non_miss_mistakes - estimated + 4.5) / (non_miss_mistakes + 4);
    // Near one miss, the score already says whether there was one break.
    estimated *= smoothstep(effective_miss_count, 1, 2);
    return estimated * non_miss_mistake_adjustment *
           logistic(missed_combo_percent, 0.33, 15);
  }

  // The deviation of presses on speed notes, taking every mistake as on
  // them.
  std::optional<double> calculate_speed_deviation() const {
    if (total_successful_hits() == 0)
      return std::nullopt;
    double speed_note_count = attributes.speed_note_count;
    speed_note_count += (total_hits() - attributes.speed_note_count) * 0.1;
    const double relevant_count_miss = net_min(count_miss, speed_note_count);
    const double relevant_count_meh =
        net_min(count_meh, speed_note_count - relevant_count_miss);
    const double relevant_count_ok = net_min(
        count_ok, speed_note_count - relevant_count_miss - relevant_count_meh);
    const double relevant_count_great =
        net_max(0, speed_note_count - relevant_count_miss - relevant_count_meh -
                       relevant_count_ok);
    return calculate_deviation(relevant_count_great, relevant_count_ok,
                               relevant_count_meh);
  }

  // The deviation 300s, 100s and 50s imply, with no mean error: 300s and
  // 100s as normally distributed, 50s as uniformly. Misses, usually aim's,
  // are left out.
  std::optional<double> calculate_deviation(double relevant_count_great,
                                            double relevant_count_ok,
                                            double relevant_count_meh) const {
    if (relevant_count_great + relevant_count_ok + relevant_count_meh <= 0)
      return std::nullopt;
    const double n = net_max(1, relevant_count_great + relevant_count_ok);
    const double p = relevant_count_great / n;
    // 99% one-tailed.
    constexpr double z = 2.32634787404;
    const double     p_lower_bound = net_min(
        p, (n * p + z * z / 2) / (n + z * z) -
               z / (n + z * z) * std::sqrt(n * p * (1 - p) + z * z / 4));
    double deviation;
    if (p_lower_bound > 0.01) {
      deviation = great_hit_window / (sqrt2 * erf_inv(p_lower_bound));
      // Less the tails outside the 100s' window: a normal distribution
      // truncated there.
      const double ok_hit_window_tail_amount =
          std::sqrt(2 / std::numbers::pi) * ok_hit_window *
          std::exp(-0.5 * pow(ok_hit_window / deviation, 2)) /
          (deviation * erf(ok_hit_window / (sqrt2 * deviation)));
      deviation *= std::sqrt(1 - ok_hit_window_tail_amount);
    } else {
      // A score of only 100s.
      deviation = ok_hit_window / std::sqrt(3);
    }
    const double meh_variance =
        (meh_hit_window * meh_hit_window + ok_hit_window * meh_hit_window +
         ok_hit_window * ok_hit_window) /
        3;
    deviation = std::sqrt(
        ((relevant_count_great + relevant_count_ok) * pow(deviation, 2) +
         relevant_count_meh * meh_variance) /
        (relevant_count_great + relevant_count_ok + relevant_count_meh));
    return deviation;
  }

  // Speed's pp past what the deviation implies it was tapped properly
  // within, grown only logarithmically.
  double speed_high_deviation_nerf() const {
    if (!speed_deviation)
      return 0;
    const double speed_value = harmonic_difficulty_to_performance(attributes.speed);
    const double excess_speed_difficulty_cutoff =
        100 + 220 * pow(22 / *speed_deviation, 6.5);
    if (speed_value <= excess_speed_difficulty_cutoff)
      return 1.0;
    constexpr double scale = 50;
    double           adjusted_speed_value =
        scale * (std::log((speed_value - excess_speed_difficulty_cutoff) / scale + 1) +
                 excess_speed_difficulty_cutoff / scale);
    // Up to 220 unstable rate counts as tapped properly.
    const double lerp = 1 - reverse_lerp(*speed_deviation, 22.0, 27.0);
    adjusted_speed_value = net_lerp(adjusted_speed_value, speed_value, lerp);
    return adjusted_speed_value / speed_value;
  }

  double traceable_bonus(double slider_factor) const {
    // Slider aim is worth less, the lower the approach rate.
    const double high_approach_rate_slider_visibility_factor =
        0.5 + (pow(slider_factor, 6) / 2);
    const double low_approach_rate_slider_visibility_factor =
        pow(slider_factor, 6);
    double bonus = 0.0275;
    bonus += 0.025 * (12.0 - net_max(approach_rate, 7)) *
             high_approach_rate_slider_visibility_factor;
    if (approach_rate < 7)
      bonus += 0.025 * (7.0 - net_max(approach_rate, 0)) *
               low_approach_rate_slider_visibility_factor;
    if (approach_rate < 0)
      bonus += 0.025 * (1 - pow(1.5, approach_rate)) *
               low_approach_rate_slider_visibility_factor;
    return bonus;
  }

  // Misses are taken to be on the hardest parts, so they cost more on maps
  // with fewer of them.
  static double miss_penalty(double miss_count, double difficult_strain_count) {
    return 0.93 /
           (miss_count / (4 * std::log(net_max(1, difficult_strain_count))) + 1);
  }

  double combo_scaling_factor() const {
    return attributes.max_combo <= 0
               ? 1.0
               : net_min(pow(score_max_combo, 0.8) /
                             pow(attributes.max_combo, 0.8),
                         1.0);
  }

  static double rate_adjusted_approach_rate(double approach_rate,
                                            double clock_rate) {
    const double preempt =
        difficulty_range(approach_rate, 1800, 1200, 450) / clock_rate;
    return inverse_difficulty_range(preempt, 1800, 1200, 450);
  }
};

}  // namespace detail

inline Performance performance(const Attributes& attributes,
                               const Beatmap&    map,
                               const Mods&       mods,
                               const Score&      score) {
  return detail::PerformanceCalculator{attributes, map, mods, score}.calculate();
}

}  // namespace osu_pp
