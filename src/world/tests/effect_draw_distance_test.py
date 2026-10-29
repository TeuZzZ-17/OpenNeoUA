from pathlib import Path
import unittest
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

class EffectDrawDistanceTest(unittest.TestCase):
    def test_emitted_particles_keep_the_world_draw_distance(self):
        source = (ROOT / 'game/yw_render.cpp').read_text()
        self.assertIn('rrg.maxZ = (float)_normalVizLimit;', source)
        self.assertIn('rrg.fadeStart = (float)(_normalVizLimit - _normalFadeLength);', source)
        self.assertIn('rrg.fadeLength = (float)_normalFadeLength;', source)
        particles = (ROOT / 'world/nparticle.cpp').read_text()
        self.assertIn('distance >= rndrParams->maxZ', particles)
        self.assertIn('rend.ColorMul.a *= distanceFade;', particles)
        self.assertIn('(p->TintAlphaAffectsAdditive ? p->Tint.a : 1.0f) * distanceFade', particles)

    def test_production_distance_fade_boundaries(self):
        source = (ROOT / 'game/yw_fx.cpp').read_text()
        begin = source.index('        const float fadeLength =', source.index('void yw_RenderTransientVPs('))
        end = source.index('        fadeFactor *= distanceFade;', begin)
        calculation = source[begin:end]
        tools = (ROOT / 'world/tools.h').read_text()
        begin = tools.index('inline float ComputeSpatialFalloff(')
        end = tools.index('\n}', begin) + 2
        program = '#include <algorithm>\n#include <cmath>\n#include <cassert>\nnamespace World {\n' + tools[begin:end] + '\n}\n'
        program += 'struct Limits { int _normalVizLimit, _normalFadeLength; };\n'
        program += 'struct Context { void *particleSystem = nullptr; };\n'
        program += 'float Evaluate(float distance, Limits *world, bool preview = false) {\nContext context; auto *arg = &context; if (preview) context.particleSystem = &context;\n' + calculation + '\nreturn distanceFade; }\n'
        program += '''int main() {
            Limits normal{5000, 2000}, changed{3000, 1000}, hard{5000, 0};
            assert(Evaluate(0, &normal) == 1);
            assert(Evaluate(3000, &normal) == 1);
            assert(Evaluate(4000, &normal) == 0.5f);
            assert(Evaluate(5000, &normal) == 0);
            assert(Evaluate(9000, &normal) == 0);
            assert(Evaluate(9000, &normal, true) == 1);
            assert(Evaluate(2500, &changed) == 0.5f);
            assert(Evaluate(4999, &hard) == 1);
            assert(Evaluate(5000, &hard) == 0);
            assert(Evaluate(4000, &normal) * 0.4f == 0.2f);
        }'''
        with tempfile.TemporaryDirectory(dir=ROOT) as folder:
            cpp, exe = Path(folder) / 'fade.cpp', Path(folder) / 'fade.exe'
            cpp.write_text(program)
            subprocess.run(['C:/msys64/mingw64/bin/g++.exe', '-std=c++14', str(cpp), '-o', str(exe)], check=True, capture_output=True)
            subprocess.run([str(exe)], check=True, capture_output=True)

    def test_curved_spatial_falloff_keeps_linear_default_and_zero_at_radius(self):
        tools = (ROOT / 'world/tools.h').read_text()
        begin = tools.index('inline float ComputeSpatialFalloff(')
        end = tools.index('inline Common::Point PositionToSectorID(const vec3d')
        program = '#include <algorithm>\n#include <cmath>\n#include <cassert>\nnamespace World {\n' + tools[begin:end] + '\n}\n'
        program += '''int main() {
            using namespace World;
            assert(ComputeSpatialFalloffCurved(1750, 3500, 1.0f) == 0.5f);
            assert(ComputeSpatialFalloffCurved(0, 3500, 0.5f) == 1.0f);
            assert(ComputeSpatialFalloffCurved(3500, 3500, 0.5f) == 0.0f);
            assert(ComputeSpatialFalloffCurved(4000, 3500, 0.5f) == 0.0f);
            assert(ComputeSpatialFalloffCurved(3000, 3500, 0.5f) > ComputeSpatialFalloffCurved(3000, 3500, 1.0f));
            assert(ComputeSpatialFalloffCurved(3000, 3500, 2.0f) < ComputeSpatialFalloffCurved(3000, 3500, 1.0f));
            assert(ComputeSpatialFalloffCurved(10, 0, 0.5f) == 1.0f);
        }'''
        with tempfile.TemporaryDirectory(dir=ROOT) as folder:
            cpp, exe = Path(folder) / 'curve.cpp', Path(folder) / 'curve.exe'
            cpp.write_text(program)
            subprocess.run(['C:/msys64/mingw64/bin/g++.exe', '-std=c++14', str(cpp), '-o', str(exe)], check=True, capture_output=True)
            subprocess.run([str(exe)], check=True, capture_output=True)

    def test_transient_fade_uses_current_world_distance(self):
        source = (ROOT / 'game/yw_fx.cpp').read_text()
        body = source[source.index('void yw_RenderTransientVPs('):]
        self.assertIn('World::ComputeSpatialFalloff(', body)
        self.assertIn('world->_normalVizLimit', body)
        self.assertIn('world->_normalFadeLength', body)
        self.assertIn('fadeFactor *= distanceFade;', body)
        self.assertIn('renderTint.a *= distanceFade;', body)
        self.assertLess(body.index('fadeFactor *= distanceFade;'), body.index('arg->vpFadeFactor = fadeFactor;'))
        self.assertIn('it->age += arg->frameTime;', body)

if __name__ == '__main__':
    unittest.main()
