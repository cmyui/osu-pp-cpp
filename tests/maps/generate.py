"""Writes the test maps: original, made-up osu!standard maps whose patterns
cover what the calculation reads (jumps, streams, rhythm changes, stacks,
every slider curve, repeats, ticks, slider velocity changes, spinners and a
break), at a few settings. Deterministic: run it again to get the same maps.

Usage: python3 generate.py    writes the .osu files next to this script
"""

import math
import os
import random

HERE = os.path.dirname(os.path.abspath(__file__))


def write(name, settings, timing, objects):
    lines = [
        'osu file format v14', '',
        '[General]', 'AudioFilename: audio.mp3', 'Mode: 0', f"StackLeniency: {settings['stack']}", '',
        '[Metadata]', f'Title:{name}', 'Artist:osu-pp-cpp', 'Creator:osu-pp-cpp', f'Version:{name}', '',
        '[Difficulty]',
        f"HPDrainRate:5", f"CircleSize:{settings['cs']}", f"OverallDifficulty:{settings['od']}",
        f"ApproachRate:{settings['ar']}", f"SliderMultiplier:{settings['sv']}", f"SliderTickRate:{settings['tick']}", '',
        '[TimingPoints]', *timing, '',
        '[HitObjects]', *objects, '',
    ]
    with open(os.path.join(HERE, f'{name}.osu'), 'w') as f:
        f.write('\n'.join(lines))


class Map:
    def __init__(self, seed, beat):
        self.random = random.Random(seed)
        self.beat = beat
        self.time = 1000.0
        self.objects = []
        self.x, self.y = 256, 192
        self.combo = 0

    def at(self):
        return round(self.time)

    def type(self, base):
        self.combo += 1
        if self.combo >= 8:
            self.combo = 0
            return base | 4
        return base

    def circle(self, x, y, gap):
        self.x, self.y = int(max(0, min(512, x))), int(max(0, min(384, y)))
        self.objects.append(f'{self.x},{self.y},{self.at()},{self.type(1)},0,0:0:0:0:')
        self.time += gap * self.beat

    def slider(self, curve, points, slides, length, gap):
        path = '|'.join([curve] + [f'{int(px)}:{int(py)}' for px, py in points])
        self.objects.append(f'{self.x},{self.y},{self.at()},{self.type(2)},0,{path},{slides},{length}')
        self.time += gap * self.beat

    def spinner(self, beats):
        end = self.time + beats * self.beat
        self.objects.append(f'256,192,{self.at()},12,0,{round(end)},0:0:0:0:')
        self.time = end + self.beat

    def jumps(self, count, spacing, gap):
        for i in range(count):
            angle = self.random.uniform(0, 2 * math.pi)
            self.circle(256 + math.cos(angle) * spacing, 192 + math.sin(angle) * spacing * 0.7, gap)

    def stream(self, count, step, gap):
        angle = self.random.uniform(0, 2 * math.pi)
        for i in range(count):
            angle += 0.15
            self.circle(self.x + math.cos(angle) * step, self.y + math.sin(angle) * step, gap)

    def triangles(self, count, size, gap):
        for i in range(count):
            angle = i * 2 * math.pi / 3
            self.circle(256 + math.cos(angle) * size, 192 + math.sin(angle) * size, gap)

    def back_and_forth(self, count, spacing, gap):
        for i in range(count):
            self.circle(256 + (spacing if i % 2 else -spacing), 192, gap)

    def stack(self, count, gap):
        for i in range(count):
            self.circle(self.x, self.y, gap)

    def sliders(self, gap):
        x, y = self.x, self.y
        self.slider('L', [(x + 120, y)], 1, 120, gap)
        self.circle(x + 200, y + 60, gap)
        self.slider('B', [(self.x + 60, self.y - 80), (self.x + 140, self.y + 40), (self.x + 180, self.y - 20)], 1, 220, gap)
        self.circle(self.x - 150, self.y + 30, gap)
        self.slider('P', [(self.x + 50, self.y - 50), (self.x + 100, self.y)], 2, 157, gap * 2)
        self.circle(self.x + 30, self.y + 120, gap)
        self.slider('L', [(self.x + 40, self.y)], 4, 40, gap * 2)  # repeats
        self.circle(self.x - 60, self.y - 90, gap)
        self.slider('L', [(self.x + 10, self.y + 5)], 1, 10, gap / 2)  # a kickslider
        self.slider('C', [(self.x + 40, self.y + 60), (self.x + 90, self.y - 10), (self.x + 140, self.y + 50)], 1, 200, gap)


def patterns():
    m = Map(1, 333.3333333333)
    m.jumps(24, 180, 1)
    m.stream(32, 22, 0.25)
    m.triangles(12, 150, 0.5)
    m.stack(6, 0.5)
    m.back_and_forth(16, 140, 0.5)
    for gap in (1, 1 / 3, 0.25, 0.5, 0.25, 1 / 3):  # rhythm changes
        m.jumps(6, 120, gap)
    m.sliders(1)
    m.spinner(8)
    m.time += 8000  # a break
    m.sliders(0.5)
    m.stream(48, 18, 0.25)
    m.jumps(30, 220, 0.5)
    timing = ['0,333.333333333333,4,2,0,100,1,0', f'{round(m.time * 0.6)},-50,4,2,0,100,0,0']
    write('patterns', {'cs': 4, 'ar': 9.3, 'od': 8.5, 'sv': 1.8, 'tick': 1, 'stack': 0.7}, timing, m.objects)


def slow():
    m = Map(2, 600)
    for _ in range(6):
        m.jumps(10, 160, 1)
        m.sliders(1)
        m.stack(3, 1)
    m.spinner(4)
    timing = ['0,600,4,2,0,100,1,0', '20000,-200,4,2,0,100,0,0', '40000,-66.6666666666667,4,2,0,100,0,0']
    write('slow', {'cs': 2, 'ar': 5, 'od': 4, 'sv': 1.4, 'tick': 0.5, 'stack': 0.5}, timing, m.objects)


def dense():
    m = Map(3, 272.727272727273)  # 220 bpm
    for _ in range(4):
        m.stream(64, 12, 0.25)
        m.jumps(16, 250, 0.5)
        m.sliders(0.5)
        m.back_and_forth(12, 60, 0.25)
    timing = ['0,272.727272727273,4,2,0,100,1,0', '8000,-40,4,2,0,100,0,0']
    write('dense', {'cs': 6, 'ar': 10, 'od': 10, 'sv': 2.6, 'tick': 2, 'stack': 0.8}, timing, m.objects)


if __name__ == '__main__':
    patterns()
    slow()
    dense()
