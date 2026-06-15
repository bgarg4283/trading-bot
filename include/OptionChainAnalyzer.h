#pragma once
#include "TradingBot.h"
#include <vector>
#include <string>

class OptionChainAnalyzer {
public:
    static OptionFlowSignal analyze(const std::vector<OptionChainEntry>& chain,
                                    double underlyingSpot,
                                    int atmRange = 10);

    static double      calcPCR_OI    (const std::vector<OptionChainEntry>& chain);
    static double      calcPCR_Volume(const std::vector<OptionChainEntry>& chain);
    static int         calcMaxPain   (const std::vector<OptionChainEntry>& chain);
    static int         getATMStrike  (const std::vector<OptionChainEntry>& chain, double spot);
    static int         getMaxOICallStrike(const std::vector<OptionChainEntry>& chain);
    static int         getMaxOIPutStrike (const std::vector<OptionChainEntry>& chain);
    static double      calcIVSkew    (const std::vector<OptionChainEntry>& chain, double spot);
    static std::string classifyFlow  (const std::vector<OptionChainEntry>& chain,
                                      double spot, int atmRange = 5);
    static double      getFlowScore  (const OptionFlowSignal& signal);

    static OptionChainEntry selectBestStrike(const std::vector<OptionChainEntry>& chain,
                                             double spot,
                                             const std::string& direction,
                                             const RiskParams& risk);

    static bool   greeksOK(const OptionGreeks& g, const RiskParams& risk,
                           const std::string& optType);
    static double estimateDelta(double spot, int strike,
                                double iv, double daysToExpiry,
                                const std::string& optType);
    static double bidAskImbalance(const OptionChainEntry& entry);
    static double chainImbalanceScore(const std::vector<OptionChainEntry>& chain,
                                      double spot, int atmRange = 5);
};
