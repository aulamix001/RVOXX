#include "Dynamics.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

static int checks = 0;
static void require(bool condition, const char* message)
{ ++checks; if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); } }
static std::vector<float> render(float input, rvoxx::Parameters p, int rate = 48000)
{
    std::vector<float> signal(static_cast<size_t>(rate), input);
    auto* ptr = signal.data(); rvoxx::Dynamics dsp; dsp.prepare(rate, p);
    dsp.process(&ptr, 1, rate, p); return signal;
}
int main()
{
    for (int rate : { 44100, 48000, 96000, 192000 })
    {
        const auto clean = render(0.25f, {}, rate);
        require(std::abs(clean.back() - 0.25f) < 1.0e-6f, "zero comp, disabled gate, unity gain passes audio");
        const auto quiet = render(0, {-36, -20, -6, 130}, rate);
        require(quiet.back() == 0, "silence remains zero");
        const auto gate = render(0.001f, {0, -30, 0, 130}, rate);
        require(std::abs(gate.back()) < 0.0003f, "gate attenuates below threshold");
        const auto open = render(0.3f, {0, -30, 0, 130}, rate);
        require(std::abs(open.back() - 0.3f) < 0.001f, "gate opens above threshold");
        const auto lowered = render(0.25f, {0, -80, -12, 130}, rate);
        require(std::abs(lowered.back() - 0.25f * rvoxx::toGain(-12)) < 1.0e-5f, "gain controls output level");
        const auto comp = render(0.25f, {-24, -80, 0, 130}, rate);
        require(comp.back() > 0.25f && comp.back() <= 1, "compression adds makeup without clipping");
    }
    {
        auto a = render(0.001f, {0, -80, 0, 130});
        auto b = render(0.001f, {-12, -80, 0, 130});
        auto c = render(0.001f, {-24, -80, 0, 130});
        require(a.back() < b.back() && b.back() < c.back(), "lower compression threshold increases low-level makeup");
    }
    // Regression: OFF after an enabled gate previously left about 15 dB of
    // attenuation at 192 kHz. Exercise real parameter transitions and blocks.
    for (int rate : { 44100, 48000, 96000, 192000 })
    {
        for (int block : { 13, 512, 4096 })
        {
            rvoxx::Dynamics dsp;
            const rvoxx::Parameters enabled { 0, -20, 0, 130 };
            dsp.prepare(rate, enabled);
            std::vector<float> closed(static_cast<size_t>(rate), 0.00001f);
            float* ptr = closed.data();
            dsp.process(&ptr, 1, rate, enabled);
            require(closed.back() < 0.000001f, "gate is engaged before bypass automation");
            std::vector<float> bypass(static_cast<size_t>(rate), 0.00001f);
            const rvoxx::Parameters off { 0, -80, 0, 130 };
            for (int i = 0; i < rate; i += block)
            {
                ptr = bypass.data() + i;
                dsp.process(&ptr, 1, std::min(block, rate - i), off);
            }
            require(std::abs(rvoxx::toDb(bypass.back() / 0.00001f)) < 0.01f, "gate OFF recovers unity at every sample rate and block size");
            require(bypass.front() < bypass[static_cast<size_t>(rate / 100)], "gate OFF ramps smoothly rather than jumping to unity");
            ptr = closed.data(); std::fill(closed.begin(), closed.end(), 0.00001f);
            const auto meter = dsp.process(&ptr, 1, rate, enabled);
            require(closed.back() < 0.000001f && meter.gateReduction < -10, "gate can be re-enabled after bypass");
        }
    }
    for (int block : { 1, 64, 512, 4096 })
    {
        std::vector<float> l(32768), r(32768);
        for (size_t i = 0; i < l.size(); ++i) { l[i] = std::sin(static_cast<float>(i) * 0.13f) * 4; r[i] = l[i] * 0.5f; }
        auto reference = l;
        const rvoxx::Parameters p {-36, -45, -9, 130};
        rvoxx::Dynamics dsp, mono; dsp.prepare(48000, p); mono.prepare(48000, p);
        float* ref = reference.data(); mono.process(&ref, 1, static_cast<int>(reference.size()), p);
        for (int i = 0; i < static_cast<int>(l.size()); i += block)
        {
            float* data[] { l.data() + i, r.data() + i };
            dsp.process(data, 2, std::min(block, static_cast<int>(l.size()) - i), p);
        }
        bool bounded = true, linked = true, independent = true;
        for (size_t i = 0; i < l.size(); ++i)
        {
            bounded &= std::isfinite(l[i]) && std::abs(l[i]) <= rvoxx::toGain(-9) + 1.0e-6f;
            linked &= std::abs(r[i] - l[i] * 0.5f) < 1.0e-6f;
            independent &= std::abs(l[i] - reference[i]) < 1.0e-6f;
        }
        require(bounded, "limiter respects ceiling including transients");
        require(linked, "stereo link preserves channel balance");
        require(independent, "block size does not change audio");
    }
    {
        rvoxx::Dynamics dsp; dsp.prepare(48000);
        float samples[] { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), 0.1f };
        float* ptr = samples; dsp.process(&ptr, 1, 4, {});
        for (float value : samples) require(std::isfinite(value), "nonfinite audio cannot poison detector");
    }
    {
        rvoxx::Dynamics dsp; dsp.prepare(48000);
        float sample = 0.5f; float* ptr = &sample;
        const auto before = dsp.process(&ptr, 1, 0, {});
        require(sample == 0.5f && before.reduction == 0, "empty buffers are safe");
        std::vector<float> x(48000, 0.001f); ptr = x.data();
        const auto meter = dsp.process(&ptr, 1, static_cast<int>(x.size()), { -12, -20, 0, 130 });
        require(meter.reduction < -10 && meter.energy <= 0 && meter.output[0] <= 0, "meters expose gate and compression reduction");
    }
    std::cout << "PASS: " << checks << " DSP checks\n";
}
