#include "IndicatorsEngine.h"
#include <cmath>
#include <numeric>
#include <algorithm>

double IndicatorsEngine::calcRSI(const std::vector<Candle>& candles, int period) {
    if ((int)candles.size() < period + 1) return 50.0;
    double avgGain = 0.0, avgLoss = 0.0;
    for (int i = 1; i <= period; ++i) {
        double diff = candles[i].close - candles[i-1].close;
        if (diff > 0) avgGain += diff;
        else          avgLoss -= diff;
    }
    avgGain /= period;
    avgLoss /= period;
    for (int i = period + 1; i < (int)candles.size(); ++i) {
        double diff = candles[i].close - candles[i-1].close;
        double gain = (diff > 0) ? diff : 0.0;
        double loss = (diff < 0) ? -diff : 0.0;
        avgGain = (avgGain * (period - 1) + gain) / period;
        avgLoss = (avgLoss * (period - 1) + loss) / period;
    }
    if (avgLoss == 0.0) return 100.0;
    double rs = avgGain / avgLoss;
    return 100.0 - (100.0 / (1.0 + rs));
}

double IndicatorsEngine::calcEMA(const std::vector<Candle>& candles, int period) {
    if ((int)candles.size() < period) return candles.empty() ? 0.0 : candles.back().close;
    double k = 2.0 / (period + 1);
    double ema = candles[0].close;
    for (int i = 1; i < (int)candles.size(); ++i)
        ema = candles[i].close * k + ema * (1.0 - k);
    return ema;
}

std::vector<double> IndicatorsEngine::calcEMASeries(const std::vector<Candle>& candles, int period) {
    std::vector<double> out(candles.size(), 0.0);
    if (candles.empty()) return out;
    double k = 2.0 / (period + 1);
    out[0] = candles[0].close;
    for (int i = 1; i < (int)candles.size(); ++i)
        out[i] = candles[i].close * k + out[i-1] * (1.0 - k);
    return out;
}

double IndicatorsEngine::calcVWAP(const std::vector<Candle>& candles) {
    double cumTPV = 0.0, cumVol = 0.0;
    for (auto& c : candles) {
        double tp = (c.high + c.low + c.close) / 3.0;
        cumTPV += tp * c.volume;
        cumVol += c.volume;
    }
    return (cumVol > 0) ? cumTPV / cumVol : 0.0;
}

// NOTE: c3m param kept for API compatibility — used for future 3m confirmation
TechnicalIndicators IndicatorsEngine::compute(const std::vector<Candle>& c1m,
                                               const std::vector<Candle>& /*c3m*/) {
    TechnicalIndicators t;
    if (c1m.size() < 2) return t;

    t.rsi14  = calcRSI(c1m, 14);
    t.rsi7   = calcRSI(c1m, 7);

    std::vector<Candle> prevBars(c1m.begin(), c1m.end() - 1);
    t.rsiPrev = calcRSI(prevBars, 14);

    t.ema9   = calcEMA(c1m, 9);
    t.ema20  = calcEMA(c1m, 20);
    t.ema50  = calcEMA(c1m, 50);
    t.ema200 = calcEMA(c1m, 200);
    t.vwap   = calcVWAP(c1m);

    double lastClose    = c1m.back().close;
    t.emaUp             = (t.ema9 > t.ema20);
    t.priceAboveEma     = (lastClose > t.ema20);
    t.rsiOB             = (t.rsi14 > 70.0);
    t.rsiOS             = (t.rsi14 < 30.0);
    return t;
}

bool IndicatorsEngine::rsiCrossedAbove(double now, double prev, double level) {
    return (prev < level && now >= level);
}

bool IndicatorsEngine::rsiCrossedBelow(double now, double prev, double level) {
    return (prev > level && now <= level);
}

bool IndicatorsEngine::emaBullishCross(const std::vector<Candle>& candles, int fast, int slow) {
    if ((int)candles.size() < slow + 2) return false;
    auto fastS = calcEMASeries(candles, fast);
    auto slowS = calcEMASeries(candles, slow);
    int n = (int)candles.size() - 1;
    return (fastS[n] > slowS[n] && fastS[n-1] <= slowS[n-1]);
}

bool IndicatorsEngine::emaBearishCross(const std::vector<Candle>& candles, int fast, int slow) {
    if ((int)candles.size() < slow + 2) return false;
    auto fastS = calcEMASeries(candles, fast);
    auto slowS = calcEMASeries(candles, slow);
    int n = (int)candles.size() - 1;
    return (fastS[n] < slowS[n] && fastS[n-1] >= slowS[n-1]);
}

// ── StreamingEMA ──────────────────────────────────────────────────────────────
IndicatorsEngine::StreamingEMA::StreamingEMA(int period)
    : period_(period), k_(2.0 / (period + 1)) {}

void IndicatorsEngine::StreamingEMA::update(double price) {
    if (count_ == 0) ema_ = price;
    else             ema_ = price * k_ + ema_ * (1.0 - k_);
    ++count_;
}

// ── StreamingRSI ──────────────────────────────────────────────────────────────
IndicatorsEngine::StreamingRSI::StreamingRSI(int period) : period_(period) {}

void IndicatorsEngine::StreamingRSI::update(double price) {
    if (count_ == 0) { prevClose_ = price; ++count_; return; }
    double diff = price - prevClose_;
    double gain = (diff > 0) ? diff : 0.0;
    double loss = (diff < 0) ? -diff : 0.0;
    if (count_ <= period_) {
        avgGain_ += gain; avgLoss_ += loss;
        if (count_ == period_) { avgGain_ /= period_; avgLoss_ /= period_; }
    } else {
        avgGain_ = (avgGain_ * (period_ - 1) + gain) / period_;
        avgLoss_ = (avgLoss_ * (period_ - 1) + loss) / period_;
        rsi_ = (avgLoss_ == 0.0) ? 100.0 : 100.0 - (100.0 / (1.0 + avgGain_ / avgLoss_));
    }
    prevClose_ = price;
    ++count_;
}
