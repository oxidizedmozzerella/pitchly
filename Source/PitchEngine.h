// Pitchly - pitch correction engine (no JUCE dependency, so it can be unit-tested anywhere)
//
// Pipeline:  YIN pitch detection (decimated)  ->  snap to allowed notes  ->  retune-speed smoothing
//            ->  TD-PSOLA style pitch-synchronous grain resynthesis (with optional formant scaling)
//
// All memory is allocated in prepare(); process() is real-time safe.
#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace pitchly
{
struct EngineParams
{
    float retuneMs     = 12.0f;  // 0 = instant (hard tune) .. 200 ms (gentle)
    float humanize     = 0.4f;   // 0..1  (widens dead-zone + slows retune a little)
    float amount       = 1.0f;   // 0..1  (how much of the correction is applied)
    float formantSemis = 0.0f;   // -6..+6
    float mix          = 0.8f;   // 0..1  dry/wet
    bool  active       = true;
    bool  notes[12]    = { true, false, true, false, true, true, false, true, false, true, false, true }; // C major
};

class PitchEngine
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kHistory     = 512;   // telemetry points kept for the UI graph

    void prepare (double sampleRate, int numChannels)
    {
        sr  = sampleRate;
        nch = std::max (1, std::min (numChannels, kMaxChannels));

        maxPeriod = (int) std::ceil (sr / kMinHz) + 2;
        latency   = (int) std::ceil (3.0 * maxPeriod) + 8;

        int ringSize = 1024;
        while (ringSize < latency + 6 * maxPeriod) ringSize <<= 1;
        mask = ringSize - 1;
        for (int c = 0; c < kMaxChannels; ++c)
        {
            inRing[c].assign ((size_t) ringSize, 0.0f);
            outRing[c].assign ((size_t) ringSize, 0.0f);
        }

        decim    = std::max (1, (int) std::lround (sr / 12000.0));
        srd      = sr / decim;
        tauMaxD  = (int) std::ceil (srd / kMinHz) + 2;
        tauMinD  = std::max (2, (int) std::floor (srd / 900.0));
        winD     = tauMaxD * 2;
        decimTotal = winD + tauMaxD + 1;
        int dsz = 1024;
        while (dsz < decimTotal + 8) dsz <<= 1;
        decimMask = dsz - 1;
        decimRing.assign ((size_t) dsz, 0.0f);
        linear.assign ((size_t) decimTotal, 0.0f);
        diff.assign ((size_t) tauMaxD + 2, 0.0f);
        cmnd.assign ((size_t) tauMaxD + 2, 0.0f);

        hop = std::max (16, (int) std::lround (sr * 0.005));
        reset();
    }

    int getLatencySamples() const { return latency; }

    void reset()
    {
        for (int c = 0; c < kMaxChannels; ++c)
        {
            std::fill (inRing[c].begin(), inRing[c].end(), 0.0f);
            std::fill (outRing[c].begin(), outRing[c].end(), 0.0f);
        }
        std::fill (decimRing.begin(), decimRing.end(), 0.0f);
        n = 0; decimWritten = 0; decimCount = 0; decimAcc = 0.0f; sinceHop = 0;
        s = 0.0; a = 0.0;
        period = sr / 200.0; voiced = false; corrSemis = 0.0; appliedSemis = 0.0;
        ratio = 1.0; fScale = 1.0; actSmooth = 1.0f;
        histWrite.store (0);
        for (int i = 0; i < kHistory; ++i) { histDetected[i].store (NAN); histCorrected[i].store (NAN); }
    }

    // channels: in-place. Only the first kMaxChannels are processed; the host is expected to pass the same count as prepare().
    void process (float* const* ch, int numCh, int numSamples, const EngineParams& p)
    {
        const int C = std::min (numCh, nch);
        for (int i = 0; i < numSamples; ++i)
        {
            float mono = 0.0f;
            for (int c = 0; c < C; ++c)
            {
                inRing[c][(size_t) (n & mask)] = ch[c][i];
                mono += ch[c][i];
            }
            mono /= (float) C;

            decimAcc += mono;
            if (++decimCount >= decim)
            {
                decimRing[(size_t) (decimWritten & decimMask)] = decimAcc / (float) decim;
                ++decimWritten; decimAcc = 0.0f; decimCount = 0;
            }
            if (++sinceHop >= hop) { sinceHop = 0; analyse (p); }

            ++n;
            const int64_t tOut = n - 1 - latency;

            // synthesise every grain whose left edge has reached the output position
            int guard = 0;
            while (s - 1.45 * period <= (double) tOut && guard++ < 8)
                synthGrain (C, tOut, p);

            actSmooth += ((p.active ? 1.0f : 0.0f) - actSmooth) * 0.002f;
            const float mixEff = p.mix * actSmooth;

            for (int c = 0; c < C; ++c)
            {
                float wet = 0.0f, dry = 0.0f;
                if (tOut >= 0)
                {
                    float& slot = outRing[c][(size_t) (tOut & mask)];
                    wet = slot; slot = 0.0f;
                    dry = inRing[c][(size_t) (tOut & mask)];
                }
                ch[c][i] = dry + (wet - dry) * mixEff;
            }
        }
    }

    // UI telemetry (lock-free; may be read from the message thread)
    std::atomic<uint32_t> histWrite { 0 };
    std::atomic<float> histDetected[kHistory];   // detected MIDI note (NaN = unvoiced)
    std::atomic<float> histCorrected[kHistory];  // detected + applied correction

private:
    static constexpr double kMinHz = 80.0;

    // ---- pitch detection (YIN) on a ~12 kHz decimated copy of the input -------------------
    bool detect (double& periodDec, float& rms)
    {
        if (decimWritten < decimTotal) return false;
        for (int i = 0; i < decimTotal; ++i)
            linear[(size_t) i] = decimRing[(size_t) ((decimWritten - decimTotal + i) & decimMask)];

        double e = 0.0;
        for (int i = decimTotal - winD; i < decimTotal; ++i) e += (double) linear[(size_t) i] * linear[(size_t) i];
        rms = (float) std::sqrt (e / winD);

        for (int tau = 1; tau <= tauMaxD; ++tau)
        {
            double d = 0.0;
            for (int j = 0; j < winD; ++j)
            {
                const double df = (double) linear[(size_t) j] - (double) linear[(size_t) (j + tau)];
                d += df * df;
            }
            diff[(size_t) tau] = (float) d;
        }
        cmnd[0] = 1.0f;
        double run = 0.0;
        for (int tau = 1; tau <= tauMaxD; ++tau)
        {
            run += diff[(size_t) tau];
            cmnd[(size_t) tau] = run > 1e-12 ? (float) (diff[(size_t) tau] * tau / run) : 1.0f;
        }

        int est = -1;
        for (int tau = tauMinD; tau < tauMaxD; ++tau)
        {
            if (cmnd[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < tauMaxD && cmnd[(size_t) (tau + 1)] < cmnd[(size_t) tau]) ++tau;
                est = tau; break;
            }
        }
        if (est < 0)
        {
            int best = tauMinD;
            for (int tau = tauMinD; tau < tauMaxD; ++tau)
                if (cmnd[(size_t) tau] < cmnd[(size_t) best]) best = tau;
            if (cmnd[(size_t) best] < 0.25f) est = best; else return false;
        }
        double ref = est;
        if (est > 1 && est < tauMaxD - 1)
        {
            const double s0 = cmnd[(size_t) (est - 1)], s1 = cmnd[(size_t) est], s2 = cmnd[(size_t) (est + 1)];
            const double den = 2.0 * (s0 - 2.0 * s1 + s2);
            if (std::fabs (den) > 1e-9) ref += std::max (-1.0, std::min (1.0, (s0 - s2) / den));
        }
        periodDec = ref;
        return true;
    }

    void analyse (const EngineParams& p)
    {
        double perDec = 0.0; float rms = 0.0f;
        const bool found = detect (perDec, rms) && rms > 0.004f;
        double detMidi = NAN;

        if (found)
        {
            const double pNew = perDec * decim;
            period = voiced ? period + 0.6 * (pNew - period) : pNew;
            period = std::max ((double) (sr / 1000.0), std::min (period, (double) maxPeriod - 2.0));
            voiced = true;
        }
        else voiced = false;

        double target = 0.0;
        if (voiced)
        {
            detMidi = 69.0 + 12.0 * std::log2 ((sr / period) / 440.0);
            const int base = (int) std::floor (detMidi + 0.5);
            double best = 1e9; int bestNote = -1;
            for (int k = -7; k <= 7; ++k)
            {
                const int cand = base + k;
                if (p.notes[((cand % 12) + 12) % 12])
                {
                    const double d = std::fabs ((double) cand - detMidi);
                    if (d < best) { best = d; bestNote = cand; }
                }
            }
            if (bestNote >= 0)
            {
                const double err  = (double) bestNote - detMidi;
                const double dead = 0.35 * p.humanize;
                const double mag  = std::max (0.0, std::fabs (err) - dead);
                target = err < 0 ? -mag : mag;
            }
        }

        const double hopSec = (double) hop / sr;
        const double tau = std::max (0.0f, p.retuneMs) / 1000.0 * (1.0 + 3.0 * p.humanize);
        const double alpha = tau < 1e-4 ? 1.0 : 1.0 - std::exp (-hopSec / tau);
        corrSemis += alpha * (target - corrSemis);

        appliedSemis = std::max (-6.0, std::min (6.0, corrSemis * p.amount));
        ratio  = std::pow (2.0, appliedSemis / 12.0);
        fScale = std::pow (2.0, std::max (-6.0f, std::min (6.0f, p.formantSemis)) / 12.0);

        const uint32_t w = histWrite.load (std::memory_order_relaxed);
        histDetected[w % kHistory].store ((float) detMidi, std::memory_order_relaxed);
        histCorrected[w % kHistory].store (voiced ? (float) (detMidi + appliedSemis) : (float) NAN, std::memory_order_relaxed);
        histWrite.store (w + 1, std::memory_order_release);
    }

    // ---- grain resynthesis -------------------------------------------------------------------
    float readInterp (int c, double pos) const
    {
        const double newest = (double) (n - 1);
        if (pos < 0.0) return 0.0f;
        if (pos > newest - 1.0) pos = newest - 1.0;
        const int64_t i0 = (int64_t) pos;
        const float fr = (float) (pos - (double) i0);
        const float x0 = inRing[c][(size_t) (i0 & mask)];
        const float x1 = inRing[c][(size_t) ((i0 + 1) & mask)];
        return x0 + (x1 - x0) * fr;
    }

    void synthGrain (int C, int64_t tOut, const EngineParams&)
    {
        const double P  = period;
        const double f  = fScale;
        const double Ps = P / ratio;                // synthesis spacing -> output pitch
        const double halfL = P / f;                 // grain half length in the output
        const double gain  = Ps * f / P;            // overlap normalisation
        const double pi = 3.14159265358979323846;

        int64_t o0 = (int64_t) std::ceil (s - halfL);
        const int64_t o1 = (int64_t) std::floor (s + halfL);
        o0 = std::max (o0, std::max<int64_t> (tOut, 0));

        for (int c = 0; c < C; ++c)
        {
            for (int64_t o = o0; o <= o1; ++o)
            {
                const double j = (double) o - s;
                const double w = 0.5 * (1.0 + std::cos (pi * j / halfL));
                outRing[c][(size_t) (o & mask)] += (float) (gain * w) * readInterp (c, a + j * f);
            }
        }

        const double sNext = s + Ps;
        const double m = std::max (0.0, std::floor ((sNext - a) / P + 0.5));
        double aNext = a + m * P;
        if (std::fabs (aNext - sNext) > 1.5 * P) aNext = sNext;   // re-sync if we drifted
        s = sNext; a = aNext;
    }

    // state
    double sr = 48000.0; int nch = 2;
    int maxPeriod = 600, latency = 1800, mask = 0;
    std::vector<float> inRing[kMaxChannels], outRing[kMaxChannels];
    int64_t n = 0;
    double s = 0.0, a = 0.0, period = 240.0, corrSemis = 0.0, appliedSemis = 0.0, ratio = 1.0, fScale = 1.0;
    bool voiced = false;
    float actSmooth = 1.0f;

    int decim = 4, tauMaxD = 152, tauMinD = 13, winD = 304, decimTotal = 457, decimMask = 1023, hop = 240, sinceHop = 0, decimCount = 0;
    double srd = 12000.0; float decimAcc = 0.0f; int64_t decimWritten = 0;
    std::vector<float> decimRing, linear, diff, cmnd;
};
} // namespace pitchly
