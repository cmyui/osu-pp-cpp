// Compares osu-pp-cpp with osu!'s own calculation: each argument is a dump
// from tools/reference (a map's playable objects with a play's mods, and
// what osu! gave for them). The objects are rated here and every value is
// checked against osu!'s, which it must equal exactly.
//
// Usage: compare <dump.txt>...    exits 1 if any value differs
#include <osu_pp/difficulty.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Reference {
  osu_pp::Beatmap                            map;
  osu_pp::Mods                               mods;
  std::map<std::string, std::string>         attributes;  // as written
  std::map<std::string, std::vector<double>> strains;
  // time, star rating, aim, speed, reading, flashlight ("none" as NaN)
  std::vector<std::vector<double>> timed;
};

double number(const std::string& text) {
  return text == "none" ? std::nan("") : std::strtod(text.c_str(), nullptr);
}
float single(const std::string& text) { return std::strtof(text.c_str(), nullptr); }

Reference read(const char* path) {
  Reference     r;
  std::ifstream file(path);
  std::string   line;
  while (std::getline(file, line)) {
    std::istringstream in(line);
    std::string        tag, a, b, c, d;
    in >> tag;
    if (tag == "difficulty") {
      in >> a >> b >> c >> d;
      r.map.circle_size = single(a);
      r.map.approach_rate = single(b);
      r.map.overall_difficulty = single(c);
      r.mods.clock_rate = number(d);
    } else if (tag == "mods") {
      std::string mod;
      while (in >> mod) {
        r.mods.hidden |= mod == "HD";
        r.mods.flashlight |= mod == "FL";
        r.mods.touch_device |= mod == "TD";
        r.mods.relax |= mod == "RX";
        r.mods.autopilot |= mod == "AP";
      }
    } else if (tag == "object") {
      osu_pp::HitObject h;
      std::string       kind, start, end, x, y;
      in >> kind >> start >> end >> x >> y >> h.stack_height;
      h.kind = kind == "s"   ? osu_pp::HitObject::slider
               : kind == "p" ? osu_pp::HitObject::spinner
                             : osu_pp::HitObject::circle;
      h.start_time = number(start);
      h.end_time = number(end);
      h.position = {single(x), single(y)};
      if (h.kind == osu_pp::HitObject::slider) {
        size_t points = 0, nested = 0;
        in >> h.repeat_count >> points;
        for (size_t i = 0; i < points; ++i) {
          std::string px, py, length;
          in >> px >> py >> length;
          h.path.points.push_back({single(px), single(py)});
          h.path.lengths.push_back(number(length));
        }
        in >> nested;
        for (size_t i = 0; i < nested; ++i) {
          std::string nk, t, nx, ny;
          in >> nk >> t >> nx >> ny;
          osu_pp::NestedObject n;
          n.kind = nk == "h"   ? osu_pp::NestedObject::head
                   : nk == "t" ? osu_pp::NestedObject::tick
                   : nk == "r" ? osu_pp::NestedObject::repeat
                               : osu_pp::NestedObject::tail;
          n.time = number(t);
          n.position = {single(nx), single(ny)};
          h.nested.push_back(n);
        }
      }
      r.map.objects.push_back(std::move(h));
    } else if (tag == "attribute") {
      in >> a >> b;
      r.attributes[a] = b;
    } else if (tag == "strains") {
      size_t n = 0;
      in >> a >> n;
      auto& values = r.strains[a];
      for (size_t i = 0; i < n; ++i) {
        in >> b;
        values.push_back(number(b));
      }
    } else if (tag == "timed") {
      size_t n = 0;
      in >> n;
      for (size_t i = 0; i < n; ++i) {
        std::vector<double> step(6);
        for (double& v : step) {
          in >> a;
          v = number(a);
        }
        r.timed.push_back(step);
      }
    }
  }
  return r;
}

// Equal, NaN with NaN.
bool same(double a, double b) { return a == b || (std::isnan(a) && std::isnan(b)); }

struct Checker {
  int    failures = 0;
  double worst = 0;  // the largest relative difference

  void check(const std::string& what, double ours, double theirs) {
    if (same(ours, theirs))
      return;
    const double relative = std::abs(ours - theirs) / std::max(std::abs(theirs), 1e-300);
    worst = std::max(worst, relative);
    if (failures++ < 5)
      std::printf("  %s: %.17g, osu! %.17g (relative %.3g)\n", what.c_str(), ours,
                  theirs, relative);
  }
};

}  // namespace

int main(int argc, char** argv) {
  int failed = 0;
  for (int i = 1; i < argc; ++i) {
    const Reference r = read(argv[i]);
    Checker         c;
    const auto      result = osu_pp::calculate(r.map, r.mods);
    const auto&     a = result.attributes;
    const std::pair<const char*, double> values[] = {
        {"star_rating", a.star_rating},
        {"aim", a.aim},
        {"aim_difficult_slider_count", a.aim_difficult_slider_count},
        {"speed", a.speed},
        {"speed_note_count", a.speed_note_count},
        {"reading", a.reading},
        {"flashlight", a.flashlight ? *a.flashlight : std::nan("")},
        {"slider_factor", a.slider_factor},
        {"aim_difficult_strain_count", a.aim_difficult_strain_count},
        {"speed_difficult_strain_count", a.speed_difficult_strain_count},
        {"reading_difficult_note_count", a.reading_difficult_note_count},
        {"aim_top_weighted_slider_factor", a.aim_top_weighted_slider_factor},
        {"speed_top_weighted_slider_factor", a.speed_top_weighted_slider_factor},
    };
    for (const auto& [name, ours] : values)
      c.check(name, ours, number(r.attributes.at(name)));
    const std::pair<const char*, const std::vector<double>*> skills[] = {
        {"aim", &result.objects.aim},
        {"aim_no_sliders", &result.objects.aim_no_sliders},
        {"speed", &result.objects.speed},
        {"reading", &result.objects.reading},
        {"flashlight", &result.objects.flashlight},
    };
    for (const auto& [name, ours] : skills) {
      const auto found = r.strains.find(name);
      const std::vector<double> none;
      const auto& theirs = found == r.strains.end() ? none : found->second;
      if (ours->size() != theirs.size()) {
        c.check(std::string(name) + " count", static_cast<double>(ours->size()),
                static_cast<double>(theirs.size()));
        continue;
      }
      for (size_t k = 0; k < theirs.size(); ++k)
        c.check(std::string(name) + " #" + std::to_string(k), (*ours)[k], theirs[k]);
    }
    if (!r.timed.empty()) {
      const auto steps = osu_pp::calculate_timed(r.map, r.mods);
      c.check("timed count", static_cast<double>(steps.size()),
              static_cast<double>(r.timed.size()));
      for (size_t k = 0; k < std::min(steps.size(), r.timed.size()); ++k) {
        const auto& s = steps[k];
        const auto& t = r.timed[k];
        const std::string at = " @" + std::to_string(k);
        c.check("timed time" + at, s.time, t[0]);
        c.check("timed star_rating" + at, s.attributes.star_rating, t[1]);
        c.check("timed aim" + at, s.attributes.aim, t[2]);
        c.check("timed speed" + at, s.attributes.speed, t[3]);
        c.check("timed reading" + at, s.attributes.reading, t[4]);
        c.check("timed flashlight" + at,
                s.attributes.flashlight ? *s.attributes.flashlight : std::nan(""), t[5]);
      }
    }
    if (c.failures) {
      std::printf("DIFFERS %s: %d values, worst relative %.3g\n", argv[i], c.failures, c.worst);
      ++failed;
    }
  }
  std::printf("%d of %d match osu! exactly\n", argc - 1 - failed, argc - 1);
  return failed ? 1 : 0;
}
