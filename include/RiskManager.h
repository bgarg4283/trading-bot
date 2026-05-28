#pragma once
#include "TradingBot.h"
#include <vector>
#include <map>
#include <mutex>

// ─── Risk Manager ────────────────────────────────────────────────────────────
// Enforces daily loss limits, per-trade SL, max concurrent positions,
// and trailing-stop logic.

class RiskManager {
public:
    explicit RiskManager(const RiskParams& params);

    // ── Pre-trade checks ─────────────────────────────────────────────────────
    struct PreTradeResult {
        bool   allowed  = false;
        int    approvedQty = 0;
        std::string reason;
    };
    PreTradeResult canEnterTrade(const TradeEntry& proposed,
                                 double availableCapital) const;

    // ── Post-fill updates ────────────────────────────────────────────────────
    void onTradeEntered(const TradeEntry& t);
    void onTradeExited (const TradeEntry& t);
    void onMTMUpdate   (const std::string& tradeId, double currentLTP);

    // ── SL / Target calculation ──────────────────────────────────────────────
    // Returns initial SL price (below entry by slPct% of premium)
    double calcInitialSL    (double entryPrice, const std::string& optType) const;
    double calcTarget1      (double entryPrice) const;
    double calcTarget2      (double entryPrice) const;

    // Trailing SL: given running high LTP, return trail SL
    double calcTrailingSL   (double runningHigh, const std::string& optType) const;

    // ── State accessors ──────────────────────────────────────────────────────
    double getDailyPnL()          const;
    double getDailyRealizedPnL()  const;
    int    getOpenTradeCount()    const;
    int    getTodayTradeCount()   const;
    bool   isDailyLimitBreached() const;
    bool   isKillSwitchActive()   const { return killSwitch_; }
    void   activateKillSwitch()         { killSwitch_ = true; }

    // Returns summary string
    std::string getSummary() const;

private:
    RiskParams params_;
    std::atomic<bool> killSwitch_{false};

    mutable std::mutex mtx_;
    std::map<std::string, TradeEntry> openTrades_;
    std::vector<TradeEntry>           closedTrades_;
    double runningMaxLTP_[/* per tradeId */100] = {};
    std::map<std::string, double> trailingHighs_;

    double totalDailyPnL_     = 0.0;
    double realizedDailyPnL_  = 0.0;
    int    todayTradeCount_   = 0;
};
