#include "RiskManager.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iostream>

RiskManager::RiskManager(const RiskParams& params) : params_(params) {}

// ─── Pre-trade check ──────────────────────────────────────────────────────────

RiskManager::PreTradeResult RiskManager::canEnterTrade(const TradeEntry& t,
                                                         double availableCapital) const {
    std::lock_guard<std::mutex> lk(mtx_);
    PreTradeResult r;

    if (killSwitch_) {
        r.reason = "KILL_SWITCH_ACTIVE"; return r;
    }
    if (isDailyLimitBreached()) {
        r.reason = "DAILY_LOSS_LIMIT"; return r;
    }
    if ((int)openTrades_.size() >= params_.maxOpenTrades) {
        r.reason = "MAX_OPEN_TRADES"; return r;
    }
    if (todayTradeCount_ >= params_.maxTradesPerDay) {
        r.reason = "MAX_DAILY_TRADES"; return r;
    }

    // Capital check: need premium * qty * lotSize
    double required = t.entryPrice * t.qty;
    if (required > availableCapital * 0.4) {  // don't use >40% capital in one trade
        r.reason = "INSUFFICIENT_CAPITAL";
        return r;
    }

    r.allowed     = true;
    r.approvedQty = t.qty;
    r.reason      = "OK";
    return r;
}

// ─── SL / Target ─────────────────────────────────────────────────────────────

double RiskManager::calcInitialSL(double entryPrice, const std::string& /*optType*/) const {
    return entryPrice * (1.0 - params_.slPct);
}

double RiskManager::calcTarget1(double entryPrice) const {
    return entryPrice * (1.0 + params_.target1Pct);
}

double RiskManager::calcTarget2(double entryPrice) const {
    return entryPrice * (1.0 + params_.target2Pct);
}

double RiskManager::calcTrailingSL(double runningHigh, const std::string& /*optType*/) const {
    return runningHigh * (1.0 - params_.trailSlPct);
}

// ─── Trade lifecycle ──────────────────────────────────────────────────────────

void RiskManager::onTradeEntered(const TradeEntry& t) {
    std::lock_guard<std::mutex> lk(mtx_);
    openTrades_[t.id] = t;
    trailingHighs_[t.id] = t.entryPrice;
    ++todayTradeCount_;
    std::cout << "[RISK] Trade entered: " << t.id << " " << t.symbol
              << " qty=" << t.qty << " @" << t.entryPrice
              << " SL=" << t.sl << " T1=" << t.target1 << " T2=" << t.target2 << "\n";
}

void RiskManager::onTradeExited(const TradeEntry& t) {
    std::lock_guard<std::mutex> lk(mtx_);
    openTrades_.erase(t.id);
    trailingHighs_.erase(t.id);
    closedTrades_.push_back(t);
    realizedDailyPnL_ += t.realizedPnL;
    totalDailyPnL_    = realizedDailyPnL_;
    std::cout << "[RISK] Trade exited: " << t.id
              << " PnL=" << std::fixed << std::setprecision(2) << t.realizedPnL
              << " Reason=" << t.exitReason << "\n";
}

void RiskManager::onMTMUpdate(const std::string& tradeId, double currentLTP) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (openTrades_.count(tradeId)) {
        auto& t = openTrades_[tradeId];
        t.mtm = (currentLTP - t.entryPrice) * t.qty;

        // Update trailing high
        if (currentLTP > trailingHighs_[tradeId])
            trailingHighs_[tradeId] = currentLTP;

        // Recompute totalDailyPnL
        totalDailyPnL_ = realizedDailyPnL_;
        for (auto& [id, trade] : openTrades_)
            totalDailyPnL_ += trade.mtm;

        // Kill switch if daily loss exceeded
        if (totalDailyPnL_ <= params_.maxDailyLoss) {
            killSwitch_ = true;
            std::cerr << "[RISK] KILL SWITCH ACTIVATED! Daily PnL: "
                      << totalDailyPnL_ << "\n";
        }
    }
}

// ─── Accessors ────────────────────────────────────────────────────────────────

double RiskManager::getDailyPnL()         const { return totalDailyPnL_; }
double RiskManager::getDailyRealizedPnL() const { return realizedDailyPnL_; }
int    RiskManager::getOpenTradeCount()   const { return (int)openTrades_.size(); }
int    RiskManager::getTodayTradeCount()  const { return todayTradeCount_; }

bool RiskManager::isDailyLimitBreached() const {
    return (totalDailyPnL_ <= params_.maxDailyLoss);
}

std::string RiskManager::getSummary() const {
    std::lock_guard<std::mutex> lk(mtx_);
    std::ostringstream os;
    os << std::fixed << std::setprecision(2);
    os << "╔══════════════════════════════════════╗\n";
    os << "║         RISK MANAGER STATUS          ║\n";
    os << "╠══════════════════════════════════════╣\n";
    os << "║ Daily PnL (MTM):  " << std::setw(10) << totalDailyPnL_    << " INR     ║\n";
    os << "║ Realized PnL:     " << std::setw(10) << realizedDailyPnL_ << " INR     ║\n";
    os << "║ Open Trades:      " << std::setw(10) << openTrades_.size() << "          ║\n";
    os << "║ Today's Trades:   " << std::setw(10) << todayTradeCount_   << "          ║\n";
    os << "║ Kill Switch:      " << (killSwitch_ ? "      ACTIVE" : "    INACTIVE") << "          ║\n";
    os << "╚══════════════════════════════════════╝\n";
    return os.str();
}
