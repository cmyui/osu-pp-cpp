// Dumps what osu!'s own difficulty calculation sees and gives for a map, for
// osu-pp-cpp's tests to compare against: the playable beatmap's objects with
// the chosen mods, as osu-pp-cpp takes them, then osu!'s difficulty
// attributes, each skill's per-object difficulties and, with --timed, the
// attributes after each object.
//
// Usage: reference [--timed] [--no-classic] <mods> <out.txt> <map.osu>
//
// <mods> is "NM", or comma-separated acronyms: HD, HR, EZ, DT, NC, HT, DC,
// FL, TD, RX, AP; a rate mod takes "@rate" (DT@1.3), and DA takes settings
// (DA:cs=7:ar=10:od=9). Classic is added unless --no-classic, as osu!'s
// servers rate maps with it.

using System.Globalization;
using System.Reflection;
using System.Text;
using osu.Game.Beatmaps;
using osu.Game.Rulesets.Difficulty;
using osu.Game.Rulesets.Difficulty.Skills;
using osu.Game.Rulesets.Mods;
using osu.Game.Rulesets.Objects;
using osu.Game.Rulesets.Osu;
using osu.Game.Rulesets.Osu.Difficulty;
using osu.Game.Rulesets.Osu.Difficulty.Skills;
using osu.Game.Rulesets.Osu.Mods;
using osu.Game.Rulesets.Osu.Objects;
using osu.Game.Utils;

var arguments = args.ToList();
bool timed = arguments.Remove("--timed");
bool classic = !arguments.Remove("--no-classic");
if (arguments.Count != 3)
{
    Console.Error.WriteLine("usage: reference [--timed] [--no-classic] <mods> <out.txt> <map.osu>");
    return 2;
}

var ruleset = new OsuRuleset();
var mods = parseMods(ruleset, arguments[0], classic);
var working = new FlatWorkingBeatmap(arguments[2]);
var beatmap = working.GetPlayableBeatmap(ruleset.RulesetInfo, mods);
var output = new StringBuilder();

string r(double value) => value.ToString("R", CultureInfo.InvariantCulture);
string f(float value) => value.ToString("R", CultureInfo.InvariantCulture);

output.AppendLine($"difficulty {f(beatmap.Difficulty.CircleSize)} {f(beatmap.Difficulty.ApproachRate)} {f(beatmap.Difficulty.OverallDifficulty)} {r(ModUtils.CalculateRateWithMods(mods))}");
output.AppendLine("mods " + string.Join(' ', mods.Select(m => m.Acronym)));

var pathField = typeof(SliderPath).GetField("calculatedPath", BindingFlags.NonPublic | BindingFlags.Instance)!;
var lengthField = typeof(SliderPath).GetField("cumulativeLength", BindingFlags.NonPublic | BindingFlags.Instance)!;

foreach (var hitObject in beatmap.HitObjects.Cast<OsuHitObject>())
{
    char kind = hitObject switch { Slider => 's', Spinner => 'p', _ => 'c' };
    output.Append($"object {kind} {r(hitObject.StartTime)} {r(hitObject.GetEndTime())} {f(hitObject.Position.X)} {f(hitObject.Position.Y)} {hitObject.StackHeight}");

    if (hitObject is Slider slider)
    {
        _ = slider.Path.Distance; // computes the path
        var points = (List<osuTK.Vector2>)pathField.GetValue(slider.Path)!;
        var lengths = (List<double>)lengthField.GetValue(slider.Path)!;
        output.Append($" {slider.RepeatCount} {points.Count}");
        for (int i = 0; i < points.Count; i++)
            output.Append($" {f(points[i].X)} {f(points[i].Y)} {r(lengths[i])}");

        output.Append($" {slider.NestedHitObjects.Count}");
        foreach (var nested in slider.NestedHitObjects.Cast<OsuHitObject>())
        {
            char nestedKind = nested switch { SliderHeadCircle => 'h', SliderTailCircle => 'e', SliderRepeat => 'r', SliderTick => 't', _ => '?' };
            output.Append($" {nestedKind} {r(nested.StartTime)} {f(nested.Position.X)} {f(nested.Position.Y)}");
        }
    }

    output.AppendLine();
}

var calculator = new CapturingCalculator(ruleset.RulesetInfo, working);
var attributes = (OsuDifficultyAttributes)calculator.Calculate(mods);
output.AppendLine($"attribute star_rating {r(attributes.StarRating)}");
output.AppendLine($"attribute aim {r(attributes.AimDifficulty)}");
output.AppendLine($"attribute aim_difficult_slider_count {r(attributes.AimDifficultSliderCount)}");
output.AppendLine($"attribute speed {r(attributes.SpeedDifficulty)}");
output.AppendLine($"attribute speed_note_count {r(attributes.SpeedNoteCount)}");
output.AppendLine($"attribute reading {r(attributes.ReadingDifficulty)}");
output.AppendLine($"attribute flashlight {(attributes.FlashlightDifficulty is double flashlight ? r(flashlight) : "none")}");
output.AppendLine($"attribute slider_factor {r(attributes.SliderFactor)}");
output.AppendLine($"attribute aim_difficult_strain_count {r(attributes.AimDifficultStrainCount)}");
output.AppendLine($"attribute speed_difficult_strain_count {r(attributes.SpeedDifficultStrainCount)}");
output.AppendLine($"attribute reading_difficult_note_count {r(attributes.ReadingDifficultNoteCount)}");
output.AppendLine($"attribute aim_top_weighted_slider_factor {r(attributes.AimTopWeightedSliderFactor)}");
output.AppendLine($"attribute speed_top_weighted_slider_factor {r(attributes.SpeedTopWeightedSliderFactor)}");

foreach (var skill in calculator.Skills)
{
    string name = skill switch
    {
        Aim { IncludeSliders: true } => "aim",
        Aim => "aim_no_sliders",
        Speed => "speed",
        Reading => "reading",
        Flashlight => "flashlight",
        _ => skill.GetType().Name,
    };
    var difficulties = skill.GetObjectDifficulties();
    output.AppendLine($"strains {name} {difficulties.Count} {string.Join(' ', difficulties.Select(r))}");
}

if (timed)
{
    var steps = new CapturingCalculator(ruleset.RulesetInfo, working).CalculateTimed(mods);
    output.Append($"timed {steps.Count}");
    foreach (var step in steps)
    {
        var a = (OsuDifficultyAttributes)step.Attributes;
        output.Append($" {r(step.Time)} {r(a.StarRating)} {r(a.AimDifficulty)} {r(a.SpeedDifficulty)} {r(a.ReadingDifficulty)} {(a.FlashlightDifficulty is double fl ? r(fl) : "none")}");
    }

    output.AppendLine();
}

File.WriteAllText(arguments[1], output.ToString());
return 0;

static Mod[] parseMods(OsuRuleset ruleset, string text, bool classic)
{
    var all = ruleset.CreateAllMods().ToList();
    var mods = new List<Mod>();

    if (text != "NM")
    {
        foreach (string part in text.Split(','))
        {
            string[] pieces = part.Split(':');
            string acronym = pieces[0].Split('@')[0];
            var mod = all.Single(m => string.Equals(m.Acronym, acronym, StringComparison.OrdinalIgnoreCase));

            if (pieces[0].Contains('@'))
                ((ModRateAdjust)mod).SpeedChange.Value = double.Parse(pieces[0].Split('@')[1], CultureInfo.InvariantCulture);

            if (mod is OsuModDifficultyAdjust adjust)
            {
                foreach (string setting in pieces.Skip(1))
                {
                    string[] pair = setting.Split('=');
                    float value = float.Parse(pair[1], CultureInfo.InvariantCulture);
                    switch (pair[0])
                    {
                        case "cs": adjust.CircleSize.Value = value; break;
                        case "ar": adjust.ApproachRate.Value = value; break;
                        case "od": adjust.OverallDifficulty.Value = value; break;
                        case "hp": adjust.DrainRate.Value = value; break;
                        default: throw new ArgumentException($"unknown setting {pair[0]}");
                    }
                }
            }

            mods.Add(mod);
        }
    }

    if (classic)
        mods.Add(new OsuModClassic());

    return mods.ToArray();
}

// osu!'s calculator, keeping the skills it rated with.
class CapturingCalculator : OsuDifficultyCalculator
{
    public Skill[] Skills = Array.Empty<Skill>();

    public CapturingCalculator(osu.Game.Rulesets.IRulesetInfo ruleset, IWorkingBeatmap beatmap)
        : base(ruleset, beatmap)
    {
    }

    protected override DifficultyAttributes CreateDifficultyAttributes(IBeatmap beatmap, Mod[] mods, Skill[] skills)
    {
        Skills = skills;
        return base.CreateDifficultyAttributes(beatmap, mods, skills);
    }
}
