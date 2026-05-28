#pragma once
#include "TradingBot.h"
#include <vector>
#include <deque>

// ─── Indicators Engine ───────────────────────────────────────────────────────
// Computes RSI, EMA, VWAP from candle vectors.
// All methods are stateless (pass candles, get result) OR
// stateful for real-time streaming (update() per new candle tick).

class IndicatorsEngine {
public:
    // ── Stateless (batch) ────────────────────────────────────────────────────

    // RSI  (Wilder's smoothed)
    static double calcRSI(const std::vector<Candle>& candles, int period = 14);

    // EMA
    static double calcEMA(const std::vector<Candle>& candles, int period);
    static std::vector<double> calcEMASeries(const std::vector<Candle>& candles, int period);

    // VWAP (intraday, resets at open)
    static double calcVWAP(const std::vector<Candle>& candles);

    // Compute full TechnicalIndicators snapshot from candle history
    static TechnicalIndicators compute(const std::vector<Candle>& candles1m,
                                       const std::vector<Candle>& candles3m);

    // ── RSI signal helpers ───────────────────────────────────────────────────
    // Returns true if RSI crossed above `level` (e.g. 30 → bullish reversal)
    static bool rsiCrossedAbove(double rsiNow, double rsiPrev, double level);
    static bool rsiCrossedBelow(double rsiNow, double rsiPrev, double level);

    // ── EMA signal helpers ───────────────────────────────────────────────────
    static bool emaBullishCross(const std::vector<Candle>& candles, int fast, int slow);
    static bool emaBearishCross(const std::vector<Candle>& candles, int fast, int slow);
    // Price vs EMA
    static bool priceAboveEMA(double price, const std::vector<Candle>& candles, int period);

    // ── Streaming (stateful) EMA ─────────────────────────────────────────────
    class StreamingEMA {
    public:
        explicit StreamingEMA(int period);
        void   update(double price);
        double value() const { return ema_; }
        bool   ready() const { return count_ >= period_; }
    private:
        int    period_;
        double k_;
        double ema_ = 0.0;
        int    count_ = 0;
    };

    // ── Streaming RSI ────────────────────────────────────────────────────────
    class StreamingRSI {
    public:
        explicit StreamingRSI(int period = 14);
        void   update(double price);
        double value() const { return rsi_; }
        bool   ready() const { return count_ >= period_ + 1; }
    private:
        int    period_;
        double avgGain_ = 0.0;
        double avgLoss_ = 0.0;
        double prevClose_ = 0.0;
        int    count_  = 0;
        double rsi_    = 50.0;
    };

private:
    static double emaMultiplier(int period) { return 2.0 / (period + 1); }
};
