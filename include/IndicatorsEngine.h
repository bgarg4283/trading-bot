#pragma once
#include "TradingBot.h"
#include <vector>

class IndicatorsEngine {
public:
    static double calcRSI(const std::vector<Candle>& candles, int period = 14);
    static double calcEMA(const std::vector<Candle>& candles, int period);
    static std::vector<double> calcEMASeries(const std::vector<Candle>& candles, int period);
    static double calcVWAP(const std::vector<Candle>& candles);

    static TechnicalIndicators compute(const std::vector<Candle>& candles1m,
                                       const std::vector<Candle>& candles3m);

    static bool rsiCrossedAbove(double rsiNow, double rsiPrev, double level);
    static bool rsiCrossedBelow(double rsiNow, double rsiPrev, double level);
    static bool emaBullishCross(const std::vector<Candle>& candles, int fast, int slow);
    static bool emaBearishCross(const std::vector<Candle>& candles, int fast, int slow);

    class StreamingEMA {
    public:
        explicit StreamingEMA(int period);
        void   update(double price);
        double value() const { return ema_; }
        bool   ready() const { return count_ >= period_; }
    private:
        int    period_;
        double k_;
        double ema_   = 0.0;
        int    count_ = 0;
    };

    class StreamingRSI {
    public:
        explicit StreamingRSI(int period = 14);
        void   update(double price);
        double value() const { return rsi_; }
        bool   ready() const { return count_ >= period_ + 1; }
    private:
        int    period_;
        double avgGain_   = 0.0;
        double avgLoss_   = 0.0;
        double prevClose_ = 0.0;
        int    count_     = 0;
        double rsi_       = 50.0;
    };
};
