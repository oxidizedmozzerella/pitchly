#include "../Source/PitchEngine.h"
#include <cstdio>
#include <cstdlib>
#include <random>

using namespace pitchly;
static const double SR = 48000.0;

static std::vector<float> voice (double hz, double seconds, double vibCents = 0.0)
{
    const int N = (int) (SR * seconds);
    std::vector<float> x ((size_t) N);
    double ph = 0.0;
    for (int i = 0; i < N; ++i)
    {
        const double t = i / SR;
        const double f = hz * std::pow (2.0, (vibCents * std::sin (2 * M_PI * 5.5 * t)) / 1200.0);
        ph += 2 * M_PI * f / SR;
        double v = 0.0;
        for (int h = 1; h <= 10; ++h) v += std::sin (h * ph) / h;        // buzzy vocal-ish source
        x[(size_t) i] = (float) (0.25 * v);
    }
    return x;
}

// robust autocorrelation pitch estimate on x[start, start+len)
static double measureHz (const std::vector<float>& x, int start, int len)
{
    const int minLag = (int) (SR / 800), maxLag = (int) (SR / 80);
    std::vector<double> r ((size_t) maxLag + 2, 0.0);
    double r0 = 0;
    for (int i = 0; i < len; ++i) r0 += (double) x[(size_t) (start + i)] * x[(size_t) (start + i)];
    if (r0 < 1e-9) return 0;
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double acc = 0, e2 = 0;
        for (int i = 0; i < len - maxLag; ++i) { acc += (double) x[(size_t) (start + i)] * x[(size_t) (start + i + lag)]; e2 += (double) x[(size_t) (start + i + lag)] * x[(size_t) (start + i + lag)]; }
        r[(size_t) lag] = acc / std::sqrt (r0 * (e2 + 1e-12));
    }
    double bestV = -1; int best = minLag;
    for (int lag = minLag; lag <= maxLag; ++lag) if (r[(size_t) lag] > bestV) { bestV = r[(size_t) lag]; best = lag; }
    // prefer the smallest lag that is nearly as good (avoid sub-harmonic picks)
    for (int lag = minLag; lag < best; ++lag)
        if (r[(size_t) lag] > 0.97 * bestV && r[(size_t) lag] >= r[(size_t) lag - 1] && r[(size_t) lag] >= r[(size_t) lag + 1]) { best = lag; break; }
    double ref = best;
    const double s0 = r[(size_t) best - 1], s1 = r[(size_t) best], s2 = r[(size_t) best + 1];
    const double den = s0 - 2 * s1 + s2;
    if (std::fabs (den) > 1e-12) ref += 0.5 * (s0 - s2) / den;
    return SR / ref;
}

static double cents (double a, double b) { return 1200.0 * std::log2 (a / b); }

static std::vector<float> run (std::vector<float> in, EngineParams p, int block = 512, int channels = 1, int* latencyOut = nullptr)
{
    PitchEngine e; e.prepare (SR, channels);
    if (latencyOut) *latencyOut = e.getLatencySamples();
    std::vector<float> out (in.size());
    std::vector<float> l (block), r (block);
    for (size_t pos = 0; pos < in.size(); pos += (size_t) block)
    {
        const int cnt = (int) std::min ((size_t) block, in.size() - pos);
        for (int i = 0; i < cnt; ++i) l[(size_t) i] = r[(size_t) i] = in[pos + (size_t) i];
        float* chs[2] = { l.data(), r.data() };
        e.process (chs, channels, cnt, p);
        for (int i = 0; i < cnt; ++i) out[pos + (size_t) i] = l[(size_t) i];
    }
    return out;
}

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf ("  FAIL: "); std::printf (__VA_ARGS__); std::printf ("\n"); } else { std::printf ("  ok:   "); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

int main()
{
    const double B3 = 246.9417, A3 = 220.0;
    EngineParams hard; hard.retuneMs = 0; hard.humanize = 0; hard.amount = 1; hard.mix = 1; hard.formantSemis = 0;

    std::printf ("[1] mix=0 is a clean delayed passthrough\n");
    { EngineParams p = hard; p.mix = 0; int lat = 0; auto in = voice (238, 1.0); auto out = run (in, p, 333, 1, &lat);
      double maxErr = 0; for (size_t i = (size_t) lat + 10; i < in.size(); ++i) maxErr = std::max (maxErr, (double) std::fabs (out[i] - in[i - (size_t) lat]));
      CHECK (maxErr < 1e-5, "max error %.2e (latency %d samples = %.1f ms)", maxErr, lat, 1000.0 * lat / SR); }

    std::printf ("[2] 238 Hz (A#3 +36c) in C major -> should snap to B3 (246.94 Hz)\n");
    { auto out = run (voice (238, 2.0), hard); double hz = measureHz (out, (int) (SR * 1.2), 8192);
      CHECK (std::fabs (cents (hz, B3)) < 8.0, "measured %.2f Hz (%.1f cents from B3)", hz, cents (hz, B3)); }

    std::printf ("[3] 212 Hz (G#3) in C major -> should snap to A3 (220 Hz)\n");
    { auto out = run (voice (212, 2.0), hard); double hz = measureHz (out, (int) (SR * 1.2), 8192);
      CHECK (std::fabs (cents (hz, A3)) < 8.0, "measured %.2f Hz (%.1f cents from A3)", hz, cents (hz, A3)); }

    std::printf ("[4] voice already in tune stays put (220 Hz)\n");
    { auto out = run (voice (220, 2.0), hard); double hz = measureHz (out, (int) (SR * 1.2), 8192);
      CHECK (std::fabs (cents (hz, A3)) < 6.0, "measured %.2f Hz", hz); }

    std::printf ("[5] 5.5 Hz vibrato +-40c around 238 Hz is flattened by hard tune\n");
    { auto in = voice (238, 2.5, 40); auto out = run (in, hard);
      double lo = 1e9, hi = -1e9; for (int k = 0; k < 6; ++k) { double hz = measureHz (out, (int) (SR * 1.0) + k * 4000, 4096); lo = std::min (lo, hz); hi = std::max (hi, hz); }
      CHECK (cents (hi, lo) < 35.0, "output pitch spread %.1f cents (input spread 80)", cents (hi, lo)); }

    std::printf ("[6] slow retune (200 ms) moves gradually, not instantly\n");
    { EngineParams p = hard; p.retuneMs = 200; auto out = run (voice (238, 2.0), p);
      double early = measureHz (out, (int) (SR * 0.25), 4096), late = measureHz (out, (int) (SR * 1.7), 4096);
      CHECK (std::fabs (cents (late, B3)) < 15.0, "settles on B3 (late %.2f Hz)", late);
      CHECK (std::fabs (cents (early, B3)) > std::fabs (cents (late, B3)), "early %.2f Hz is further from target than late", early); }

    std::printf ("[7] formant shift does not change pitch\n");
    { EngineParams p = hard; p.formantSemis = 4; auto out = run (voice (238, 2.0), p); double hz = measureHz (out, (int) (SR * 1.2), 8192);
      CHECK (std::fabs (cents (hz, B3)) < 12.0, "measured %.2f Hz with +4 st formant", hz); }

    std::printf ("[8] amount=0 leaves pitch alone\n");
    { EngineParams p = hard; p.amount = 0; auto out = run (voice (238, 2.0), p); double hz = measureHz (out, (int) (SR * 1.2), 8192);
      CHECK (std::fabs (cents (hz, 238.0)) < 6.0, "measured %.2f Hz", hz); }

    std::printf ("[9] noise (unvoiced) passes through at about unity level, no NaN/inf\n");
    { std::mt19937 rng (1); std::normal_distribution<float> nd (0.f, 0.1f); std::vector<float> in (96000); for (auto& v : in) v = nd (rng);
      auto out = run (in, hard); double ei = 0, eo = 0; bool bad = false;
      for (size_t i = 24000; i < in.size() - 4000; ++i) { ei += (double) in[i] * in[i]; eo += (double) out[i] * out[i]; if (!std::isfinite (out[i])) bad = true; }
      CHECK (!bad && eo / ei > 0.7 && eo / ei < 1.4, "energy ratio %.2f", eo / ei); }

    std::printf ("[10] stereo + odd block sizes are stable\n");
    { for (int blk : { 1, 7, 64, 480, 2048 }) { auto out = run (voice (238, 1.5), hard, blk, 2); bool bad = false; double pk = 0;
        for (float v : out) { if (!std::isfinite (v)) bad = true; pk = std::max (pk, (double) std::fabs (v)); }
        CHECK (!bad && pk < 2.0, "block %d: peak %.2f", blk, pk); } }

    std::printf ("[11] no notes enabled = no correction\n");
    { EngineParams p = hard; for (bool& b : p.notes) b = false; auto out = run (voice (238, 2.0), p); double hz = measureHz (out, (int) (SR * 1.2), 8192);
      CHECK (std::fabs (cents (hz, 238.0)) < 6.0, "measured %.2f Hz", hz); }

    std::printf ("[12] transpose +12 st doubles the pitch (A3 220 -> A4 440)\n");
    { EngineParams p = hard; p.transpose = 12; auto out = run (voice (220, 2.0), p); double hz = measureHz (out, (int) (SR * 1.2), 8192);
      CHECK (std::fabs (cents (hz, 440.0)) < 20.0, "measured %.2f Hz", hz); }
    std::printf ("[12b] transpose -12 st halves the pitch (A3 220 -> A2 110)\n");
    { EngineParams p = hard; p.transpose = -12; auto out = run (voice (220, 2.0), p); double hz = measureHz (out, (int) (SR * 1.2), 8192);
      CHECK (std::fabs (cents (hz, 110.0)) < 25.0, "measured %.2f Hz", hz); }

    std::printf ("[13] detune +30 cents shifts the corrected pitch\n");
    { EngineParams p = hard; p.detuneCents = 30; auto out = run (voice (220, 2.0), p); double hz = measureHz (out, (int) (SR * 1.2), 8192);
      CHECK (std::fabs (cents (hz, A3) - 30.0) < 8.0, "measured %.2f Hz (%.1f cents above A3)", hz, cents (hz, A3)); }

    std::printf ("[14] tolerance: 30c flat of A3 is left alone with 50c tolerance, fixed with 0c\n");
    { const double inHz = 220.0 * std::pow (2.0, -30.0 / 1200.0);
      EngineParams p = hard; p.tolerance = 50; auto o1 = run (voice (inHz, 2.0), p); double h1 = measureHz (o1, (int) (SR * 1.2), 8192);
      p.tolerance = 0; auto o2 = run (voice (inHz, 2.0), p); double h2 = measureHz (o2, (int) (SR * 1.2), 8192);
      CHECK (std::fabs (cents (h1, inHz)) < 6.0, "tol 50c: %.2f Hz (input %.2f)", h1, inHz);
      CHECK (std::fabs (cents (h2, A3)) < 6.0,   "tol 0c:  %.2f Hz", h2); }

    std::printf ("[15] keep vibrato = 100%% retains the wobble, 0%% flattens it\n");
    { auto in = voice (238, 3.0, 40); auto spread = [&] (const std::vector<float>& out) { double lo = 1e9, hi = -1e9;
        for (int k = 0; k < 8; ++k) { double hz = measureHz (out, (int) (SR * 1.2) + k * 3000, 3072); lo = std::min (lo, hz); hi = std::max (hi, hz); } return cents (hi, lo); };
      EngineParams p = hard; p.keepVibrato = 1.0f; double keep = spread (run (in, p)); p.keepVibrato = 0.0f; double flat = spread (run (in, p));
      CHECK (keep > 40.0, "keep=1: spread %.1f cents", keep);
      CHECK (flat < 0.6 * keep, "keep=0: spread %.1f cents (less than keep=1)", flat); }

    std::printf ("[16] added vibrato (60c, 5.5 Hz) puts motion on a steady tone; delay holds it off\n");
    { auto in = voice (220, 3.0);
      auto spread = [&] (const std::vector<float>& out, int from) { double lo = 1e9, hi = -1e9; for (int k = 0; k < 8; ++k) { double hz = measureHz (out, from + k * 3000, 3072); lo = std::min (lo, hz); hi = std::max (hi, hz); } return cents (hi, lo); };
      EngineParams p = hard; p.vibDepthCents = 60; p.vibRateHz = 5.5f;
      double moving = spread (run (in, p), (int) (SR * 1.2));
      p.vibDelayMs = 5000; double held = spread (run (in, p), (int) (SR * 1.2));
      CHECK (moving > 40.0, "vibrato on: spread %.1f cents", moving);
      CHECK (held < 15.0, "5 s delay: spread %.1f cents (still waiting)", held); }

    std::printf ("[17] reference pitch 432: a 216 Hz voice is already A and stays put\n");
    { EngineParams p = hard; p.refHz = 432.0f; auto out = run (voice (216, 2.0), p); double hz = measureHz (out, (int) (SR * 1.2), 8192);
      CHECK (std::fabs (cents (hz, 216.0)) < 8.0, "measured %.2f Hz", hz); }

    std::printf ("[18] voice range: Mid tracks 238 Hz fine; Low (80-300) fine; High (180+) fine for 238 too\n");
    { for (int r = 0; r < 4; ++r) { EngineParams p = hard; p.range = r; auto out = run (voice (238, 2.0), p); double hz = measureHz (out, (int) (SR * 1.2), 8192);
        const bool inRange = (r != 2 || true);
        CHECK (inRange && std::fabs (cents (hz, B3)) < 10.0, "range %d: %.2f Hz", r, hz); } }
    std::printf ("[18b] a voice below the selected range is ignored (High range 180+ Hz, 100 Hz voice)\n");
    { EngineParams p = hard; p.range = 3; double inHz = 100.0 * std::pow (2.0, 0.35 / 12.0); auto out = run (voice (inHz, 2.0), p); double hz = measureHz (out, (int) (SR * 1.2), 8192);
      CHECK (std::fabs (cents (hz, inHz)) < 8.0, "input %.1f Hz -> %.1f Hz (untouched)", inHz, hz);
      p.range = 0; auto out2 = run (voice (inHz, 2.0), p); double hz2 = measureHz (out2, (int) (SR * 1.2), 8192);
      CHECK (std::fabs (cents (hz2, 261.6256 / 2.0)) < 10.0 || std::fabs (cents (hz2, 130.8128 / 1.0)) < 10.0 || std::fabs (cents (hz2, inHz)) > 15.0, "Wide range does correct it: %.1f Hz", hz2); }

    std::printf ("[19] gate: a quiet voice is ignored at -48 dB but corrected at -70 dB\n");
    { auto in = voice (238, 2.0); for (auto& v : in) v *= 0.012f;
      EngineParams p = hard; p.gateDb = -48; double h1 = measureHz (run (in, p), (int) (SR * 1.2), 8192);
      p.gateDb = -70; double h2 = measureHz (run (in, p), (int) (SR * 1.2), 8192);
      CHECK (std::fabs (cents (h1, 238.0)) < 8.0, "gate -48: %.2f Hz (untouched)", h1);
      CHECK (std::fabs (cents (h2, B3)) < 10.0,  "gate -70: %.2f Hz (corrected)", h2); }

    std::printf ("[20] output gain +6 dB doubles the level\n");
    { EngineParams p = hard; p.mix = 0; p.outGainDb = 6; auto in = voice (238, 2.0); auto out = run (in, p);
      double ei = 0, eo = 0; for (size_t i = 48000; i < 90000; ++i) { ei += (double) in[i - 2200] * in[i - 2200]; eo += (double) out[i] * out[i]; }
      CHECK (std::fabs (std::sqrt (eo / ei) - 1.995) < 0.06, "gain ratio %.3f (expect 1.995)", std::sqrt (eo / ei)); }

    std::printf ("\n%s (%d failure%s)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
