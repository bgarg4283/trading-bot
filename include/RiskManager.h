#pragma once
#include "TradingBot.h"
#include <vector>
#include <map>
#include <mutex>
#include <atomic>

class RiskManager {
public:
    explicit RiskManager(const RiskParams& params);

    struct PreTradeResult {
        bool        allowed    = false;
        int         approvedQty = 0;
        std::string reason;
    };
    PreTradeResult canEnterTrade(const TradeEntry& proposed,
                                 double availableCapital) const;

    void onTradeEntered(const TradeEntry& t);
    void onTradeExited (const TradeEntry& t);
    void onMTMUpdate   (const std::string& tradeId, double currentLTP);

    double calcInitialSL  (double entryPrice) const;
    double calcTarget1    (double entryPrice) const;
    double calcTarget2    (double entryPrice) const;
    double calcTrailingSL (double runningHigh) const;

    double getDailyPnL()          const;
    double getDailyRealizedPnL()  const;
    int    getOpenTradeCount()    const;
    int    getTodayTradeCount()   const;
    bool   isDailyLimitBreached() const;
    bool   isKillSwitchActive()   const { return killSwitch_.load(); }
    void   activateKillSwitch()         { killSwitch_ = true; }
    std::string getSummary() const;

private:
    RiskParams params_;
    std::atomic<bool> killSwitch_{false};
    mutable std::mutex mtx_;
    std::map<std::string, TradeEntry> openTrades_;
    std::vector<TradeEntry>           closedTrades_;
    std::map<std::string, double>     trailingHighs_;
    double totalDailyPnL_    = 0.0;
    double realizedDailyPnL_ = 0.0;
    int    todayTradeCount_  = 0;
};
