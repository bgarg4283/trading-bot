#pragma once
#include "TradingBot.h"
#include "IndicatorsEngine.h"
#include "OptionChainAnalyzer.h"
#include <vector>
#include <string>

struct StrategySignal {
    TradeSignal signal          = TradeSignal::NONE;
    Instrument  instrument      = Instrument::NIFTY50;
    std::string direction;
    OptionChainEntry selectedOption;
    double      confidenceScore = 0.0;
    std::string reason;
    bool rsiOK   = false;
    bool emaOK   = false;
    bool vwapOK  = false;
    bool flowOK  = false;
    bool greeksOK= false;
};

class StrategyEngine {
public:
    explicit StrategyEngine(const BotConfig& cfg);

    StrategySignal evaluate(Instrument inst,
                            const MarketData& spot,
                            const TechnicalIndicators& tech,
                            const OptionFlowSignal& flow,
                            const std::vector<OptionChainEntry>& chain);

    struct ExitDecision {
        bool        shouldExit = false;
        std::string reason;
        int         exitQty    = 0;
    };
    ExitDecision checkExit(const TradeEntry& trade,
                           double currentLTP,
                           const OptionGreeks& currentGreeks,
                           const TechnicalIndicators& tech);

    void   setRiskParams(const RiskParams& r) { risk_ = r; }
    double calcPositionSize(double premium, double availableCapital) const;

private:
    BotConfig  cfg_;
    RiskParams risk_;
    double prevRsi14_[2] = {50.0, 50.0};
    double prevRsi7_ [2] = {50.0, 50.0};

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
