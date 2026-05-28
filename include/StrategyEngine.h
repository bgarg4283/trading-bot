#pragma once
#include "TradingBot.h"
#include "IndicatorsEngine.h"
#include "OptionChainAnalyzer.h"
#include <vector>
#include <string>

// ─── Strategy Engine ─────────────────────────────────────────────────────────
// Combines RSI + EMA (momentum) with option chain flow & Greeks filters
// to generate SCALP signals for Nifty/FinNifty.
//
// Entry Logic (CE):
//   1. RSI(14) crosses above 40 OR RSI(7) > 55 (momentum)
//   2. EMA9 > EMA20 (short-term uptrend)
//   3. Price > VWAP
//   4. PCR < 0.85 OR PCR trending down (bullish flow)
//   5. Put writing > call writing at ATM±5 strikes
//   6. Best CE strike has delta 0.35–0.65 and IV within limits
//   7. Bid-ask spread < 2% of LTP
//
// Entry Logic (PE):
//   1. RSI(14) crosses below 60 OR RSI(7) < 45
//   2. EMA9 < EMA20
//   3. Price < VWAP
//   4. PCR > 1.20 OR PCR trending up (bearish flow)
//   5. Call writing > put writing at ATM±5 strikes
//   6. Best PE strike has delta 0.35–0.65 and IV within limits
//
// Exit Logic:
//   - SL: 30% loss on premium
//   - Target1: 40% gain → exit 50% qty
//   - Target2: 80% gain → exit remaining
//   - Trailing SL: after T1, trail at 30% below running high
//   - Time-based exit: 15:15 IST forced exit
//   - Greeks: if delta < 0.20 or IV > maxIV → exit

struct StrategySignal {
    TradeSignal signal      = TradeSignal::NONE;
    Instrument  instrument  = Instrument::NIFTY50;
    std::string direction;        // "CE" or "PE"
    OptionChainEntry selectedOption;
    double      confidenceScore = 0.0;  // 0–100
    std::string reason;

    // Sub-signals for debugging
    bool rsiOK      = false;
    bool emaOK      = false;
    bool vwapOK     = false;
    bool flowOK     = false;
    bool greeksOK   = false;
};

class StrategyEngine {
public:
    explicit StrategyEngine(const BotConfig& cfg);

    // ── Main signal generator ────────────────────────────────────────────────
    // Call every 1-min candle close or on significant tick
    StrategySignal evaluate(Instrument inst,
                            const MarketData& spot,
                            const TechnicalIndicators& tech,
                            const OptionFlowSignal& flow,
                            const std::vector<OptionChainEntry>& chain);

    // ── Exit signal check (call for each open trade) ─────────────────────────
    struct ExitDecision {
        bool        shouldExit = false;
        std::string reason;
        int         exitQty    = 0;   // partial or full
    };
    ExitDecision checkExit(const TradeEntry& trade,
                           double currentLTP,
                           const OptionGreeks& currentGreeks,
                           const TechnicalIndicators& tech);

    // ── Helpers ──────────────────────────────────────────────────────────────
    void  setRiskParams(const RiskParams& r) { risk_ = r; }
    double calcPositionSize(double premium, double availableCapital) const;

private:
    BotConfig  cfg_;
    RiskParams risk_;

    // Previous-candle state for cross detection
    double prevRsi14_[2]  = {50.0, 50.0};   // [nifty, finnifty]
    double prevRsi7_[2]   = {50.0, 50.0};

    bool checkCEEntry(const MarketData& spot,
                      const TechnicalIndicators& tech,
                      const OptionFlowSignal& flow,
                      StrategySignal& out) const;

    bool checkPEEntry(const MarketData& spot,
                      const TechnicalIndicators& tech,
                      const OptionFlowSignal& flow,
                      StrategySignal& out) const;

    double scoreSignal(const StrategySignal& sig,
                       const OptionFlowSignal& flow) const;
};
