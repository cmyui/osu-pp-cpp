#!/bin/sh
# Checks osu-pp-cpp against osu!'s own calculation: builds the reference
# (tools/reference, needs the .NET 10 SDK) and the comparison, dumps each
# map with each set of mods, and compares every value exactly.
#
# Usage: tests/run.sh [map.osu or directory]...    defaults to tests/maps
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
work=${TMPDIR:-/tmp}/osu-pp-cpp-tests
rm -rf "$work" && mkdir -p "$work/dumps"

dotnet build "$root/tools/reference" -c Release -nologo -v q
reference="$root/tools/reference/bin/Release/net10.0/Reference.dll"
cmake -S "$root" -B "$work/build" -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build "$work/build" --target compare > /dev/null

[ $# -gt 0 ] || set -- "$root/tests/maps"
find "$@" -name '*.osu' | while IFS= read -r map; do
  name=$(basename "$map" .osu | tr -c 'A-Za-z0-9\n' '_')
  for mods in NM HD,HR DT FL,HD EZ,HT TD RX AP DA:cs=7:ar=10.5:od=9 DT@1.25 NF,SO SV2 BL TC NC,HD,FL; do
    dotnet "$reference" --timed "$mods" "$work/dumps/${name}_$(echo "$mods" | tr ':,=@.' '_____').txt" "$map"
  done
  # Scores set on osu!lazer without Classic: their slider tails and ticks
  # judged, not estimated.
  for mods in NM HD,DT; do
    dotnet "$reference" --no-classic "$mods" "$work/dumps/${name}_lazer_$(echo "$mods" | tr ':,=@.' '_____').txt" "$map"
  done
done
"$work/build/compare" "$work"/dumps/*.txt
