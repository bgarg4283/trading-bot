#pragma once
#include "TradingBot.h"
#include <vector>
#include <map>
#include <string>

// ─── Option Chain Analyzer ───────────────────────────────────────────────────
// Decodes option chain data to extract market flow, PCR, max pain,
// support/resistance, IV skew and dominant activity.

class OptionChainAnalyzer {
public:
    // Main entry point – returns a filled OptionFlowSignal
    static OptionFlowSignal analyze(const std::vector<OptionChainEntry>& chain,
                                    double underlyingSpot,
                                    int    atmRange = 10);   // strikes around ATM

    // ── Individual metrics ───────────────────────────────────────────────────

    // Put/Call ratio by OI
    static double calcPCR_OI(const std::vector<OptionChainEntry>& chain);

    // Put/Call ratio by volume
    static double calcPCR_Volume(const std::vector<OptionChainEntry>& chain);

    // Max pain strike (strike where total notional loss for option buyers is max)
    static int calcMaxPain(const std::vector<OptionChainEntry>& chain);

    // ATM strike (nearest to spot)
    static int getATMStrike(const std::vector<OptionChainEntry>& chain, double spot);

    // Highest OI call strike (resistance) and put strike (support)
    static int getMaxOICallStrike(const std::vector<OptionChainEntry>& chain);
    static int getMaxOIPutStrike (const std::vector<OptionChainEntry>& chain);

    // IV skew: avg IV of OTM puts vs OTM calls
    static double calcIVSkew(const std::vector<OptionChainEntry>& chain, double spot);

    // Dominant flow classification
    // "CALL_WRITING"  = big OI build in calls  → bearish
    // "PUT_WRITING"   = big OI build in puts    → bullish
    // "CALL_UNWINDING"= OI falling in calls     → mildly bullish
    // "PUT_UNWINDING" = OI falling in puts      → mildly bearish
    static std::string classifyFlow(const std::vector<OptionChainEntry>& chain,
                                    double spot,
                                    int    atmRange = 5);

    // Score-based direction: +ve = bullish, -ve = bearish
    static double getFlowScore(const OptionFlowSignal& signal);

    // ── Greeks-based filters ─────────────────────────────────────────────────

    // Find best strike to trade given direction & greeks filter
    // direction: "CE" or "PE"
    static OptionChainEntry selectBestStrike(const std::vector<OptionChainEntry>& chain,
                                             double spot,
                                             const std::string& direction,
                                             const RiskParams& risk);

    // Check if option greeks are within acceptable range for entry
    static bool greeksOK(const OptionGreeks& g, const RiskParams& risk,
                         const std::string& optType);

    // Estimate delta from moneyness (Black-Scholes approximation)
    static double estimateDelta(double spot, int strike,
                                double iv, double daysToExpiry,
                                const std::string& optType);

    // ── Imbalance ────────────────────────────────────────────────────────────
    // Bid-ask imbalance at given strike
    static double bidAskImbalance(const OptionChainEntry& entry);

    // Total OI-weighted imbalance score
    static double chainImbalanceScore(const std::vector<OptionChainEntry>& chain,
                                      double spot,
                                      int atmRange = 5);
};
