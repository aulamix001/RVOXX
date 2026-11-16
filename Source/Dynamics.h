#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace rvoxx
{
inline float toGain(float db) noexcept { return std::pow(10.0f, db * 0.05f); }
inline float toDb(float gain) noexcept { return 20.0f * std::log10(std::max(gain, 1.0e-8f)); }
struct Parameters { float compression = 0, gate = -80, gain = 0, gateRelease = 130; };
struct Meters { float energy = -80, reduction = 0; std::array<float, 2> output { -160, -160 }; };

// Independent approximation. The supplied manual does not specify detector,
// ratio, knee, time constants, makeup law, or the original limiter algorithm.
class Dynamics
{
public:
    void prepare(double sampleRate, Parameters initial = {}) noexcept
    {
        rate = std::clamp(sampleRate, 8000.0, 384000.0);
        smooth = coefficient(20);
        detectorAttack = coefficient(1);
        detectorRelease = coefficient(80);
        compAttack = coefficient(3);
        compRelease = coefficient(160);
        gateAttack = coefficient(2);
        limiterRelease = coefficient(60);
        current = initial;
        envelope = compReduction = gateReduction = 0;
        limiterGain = 1;
    }

    Meters process(float* const* data, int channels, int samples, Parameters target) noexcept
    {
        Meters meters;
        channels = std::clamp(channels, 0, 2);
        target.compression = std::clamp(target.compression, -36.0f, 0.0f);
        target.gate = std::clamp(target.gate, -80.0f, 0.0f);
        target.gain = std::clamp(target.gain, -36.0f, 0.0f);
        target.gateRelease = std::clamp(target.gateRelease, 10.0f, 250.0f);
        const float gateRelease = coefficient(target.gateRelease);
        for (int i = 0; i < samples; ++i)
        {
            current.compression = follow(current.compression, target.compression, smooth);
            current.gate = follow(current.gate, target.gate, smooth);
            current.gain = follow(current.gain, target.gain, smooth);
            float peak = 0;
            for (int c = 0; c < channels; ++c)
            {
                if (!std::isfinite(data[c][i])) data[c][i] = 0;
                peak = std::max(peak, std::abs(data[c][i]));
            }
            envelope = follow(envelope, peak, peak > envelope ? detectorAttack : detectorRelease);
            const float inputDb = toDb(envelope);
            const float gateTarget = current.gate <= -79.99f ? 0.0f
                : -std::min(80.0f, softAbove(current.gate - inputDb, 6.0f) * 0.75f);
            gateReduction = follow(gateReduction, gateTarget,
                gateTarget > gateReduction ? gateAttack : gateRelease);
            const float postGateDb = inputDb + gateReduction;
            // Increasing compression also increases ratio (1:1 -> 8:1).
            const float ratio = 1.0f + (-current.compression / 36.0f) * 7.0f;
            const float compTarget = -softAbove(postGateDb - current.compression, 6.0f) * (1.0f - 1.0f / ratio);
            compReduction = follow(compReduction, compTarget,
                compTarget < compReduction ? compAttack : compRelease);
            const float makeupDb = -current.compression * (1.0f - 1.0f / ratio);
            const float dynamicsGain = toGain(gateReduction + compReduction + makeupDb);
            // Stereo-linked, instantaneous attack peak limiter, then output ceiling.
            const float preLimitPeak = peak * dynamicsGain;
            const float wanted = preLimitPeak > 1 ? 1 / preLimitPeak : 1;
            limiterGain = wanted < limiterGain ? wanted : follow(limiterGain, wanted, limiterRelease);
            const float ceiling = toGain(current.gain);
            for (int c = 0; c < channels; ++c)
            {
                data[c][i] = std::clamp(data[c][i] * dynamicsGain * limiterGain * ceiling, -ceiling, ceiling);
                meters.output[static_cast<size_t>(c)] = std::max(meters.output[static_cast<size_t>(c)], toDb(std::abs(data[c][i])));
            }
            meters.energy = std::max(meters.energy, std::max(-80.0f, inputDb));
            meters.reduction = std::min(meters.reduction, gateReduction + compReduction);
        }
        return meters;
    }
private:
    float coefficient(float ms) const noexcept { return static_cast<float>(std::exp(-1.0 / (0.001 * ms * rate))); }
    static float follow(float old, float value, float coeff) noexcept
    { const auto result = value + coeff * (old - value); return std::abs(result) < 1.0e-20f ? 0.0f : result; }
    static float softAbove(float x, float knee) noexcept
    { if (x <= -knee * 0.5f) return 0; if (x >= knee * 0.5f) return x; const float t = x + knee * 0.5f; return t * t / (2 * knee); }
    double rate = 48000;
    float smooth = 0, detectorAttack = 0, detectorRelease = 0, compAttack = 0, compRelease = 0, gateAttack = 0, limiterRelease = 0;
    float envelope = 0, compReduction = 0, gateReduction = 0, limiterGain = 1;
    Parameters current;
};
}
