#include "TradingBot.h"
#include "FyersAPI.h"
#include "IndicatorsEngine.h"
#include "OptionChainAnalyzer.h"
#include "StrategyEngine.h"
#include "RiskManager.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <map>
#include <atomic>
#include <csignal>
#include <uuid/uuid.h>

// ─── Signal handler ───────────────────────────────────────────────────────────

static std::atomic<bool> gRunning{true};
void sigHandler(int) { gRunning = false; std::cout << "\n[BOT] Shutdown signal received.\n"; }

// ─── Helpers ─────────────────────────────────────────────────────────────────

static std::string genId() {
    uuid_t uuid;
    uuid_generate(uuid);
    char s[37];
    uuid_unparse_lower(uuid, s);
    return std::string(s).substr(0, 8);
}

static long long nowEpoch() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

static std::string nowIST() {
    time_t t = time(nullptr);
    struct tm* lt = localtime(&t);
    char buf[20];
    strftime(buf, sizeof(buf), "%H:%M:%S", lt);
    return buf;
}

static bool isMarketOpen(const BotConfig& cfg) {
    time_t t = time(nullptr);
    struct tm* lt = localtime(&t);
    int h = lt->tm_hour, m = lt->tm_min;
    int now = h * 100 + m;
    return (now >= 915 && now < 1530);
}

static bool isNoTradeTime(const BotConfig& cfg) {
    time_t t = time(nullptr);
    struct tm* lt = localtime(&t);
    int h = lt->tm_hour, m = lt->tm_min;
    int now = h * 100 + m;
    return (now >= 1515);
}

// ─── Bot State ────────────────────────────────────────────────────────────────

struct BotState {
    std::map<std::string, TradeEntry> openTrades;
    std::map<Instrument, MarketData>  spotData;
    std::map<Instrument, TechnicalIndicators> techs;
    std::map<Instrument, OptionFlowSignal>     flows;
    std::map<Instrument, std::vector<OptionChainEntry>> chains;
};

// ─── Execute Trade ────────────────────────────────────────────────────────────

void executeTrade(const StrategySignal& sig,
                  FyersAPI& api,
                  RiskManager& riskMgr,
                  BotState& state,
                  const BotConfig& cfg) {
    if (sig.signal == TradeSignal::NONE) return;
    if (sig.selectedOption.symbol.empty()) {
        std::cout << "[TRADE] No suitable strike found for " << sig.direction << "\n";
        return;
    }

    // Confidence threshold
    if (sig.confidenceScore < 60.0) {
        std::cout << "[TRADE] Confidence too low (" << sig.confidenceScore << "), skipping.\n";
        return;
    }

    int lotSize = (sig.instrument == Instrument::NIFTY50) ? cfg.niftyLotSize : cfg.finniftyLotSize;
    int lots    = 1;
    int qty     = lots * lotSize;

    TradeEntry trade;
    trade.id          = genId();
    trade.instrument  = sig.instrument;
    trade.symbol      = sig.selectedOption.symbol;
    trade.optionType  = sig.direction;
    trade.strikePrice = sig.selectedOption.strikePrice;
    trade.expiry      = sig.selectedOption.expiry;
    trade.qty         = qty;
    trade.entryPrice  = sig.selectedOption.ltp;
    trade.sl          = riskMgr.calcInitialSL(trade.entryPrice, sig.direction);
    trade.target1     = riskMgr.calcTarget1(trade.entryPrice);
    trade.target2     = riskMgr.calcTarget2(trade.entryPrice);
    trade.greeksAtEntry = sig.selectedOption.greeks;
    trade.status      = TradeStatus::PENDING;
    trade.entryTime   = nowEpoch();

    double capital = api.getAvailableMargin();
    auto preCheck  = riskMgr.canEnterTrade(trade, capital);
    if (!preCheck.allowed) {
        std::cout << "[TRADE] Blocked by risk manager: " << preCheck.reason << "\n";
        return;
    }

    // Place order
    FyersOrderRequest req;
    req.symbol = trade.symbol;
    req.side   = "1";   // BUY
    req.qty    = trade.qty;
    req.type   = "2";   // MARKET
    auto resp  = api.placeOrder(req);

    if (resp.success) {
        trade.status = TradeStatus::OPEN;
        state.openTrades[trade.id] = trade;
        riskMgr.onTradeEntered(trade);
        std::cout << "[TRADE] ✅ ENTERED " << trade.optionType << " " << trade.symbol
                  << " qty=" << trade.qty << " @" << trade.entryPrice
                  << " SL=" << trade.sl << " T1=" << trade.target1
                  << " T2=" << trade.target2
                  << " Confidence=" << sig.confidenceScore << "%\n"
                  << "        Reason: " << sig.reason << "\n";
    } else {
        std::cerr << "[TRADE] ❌ Order failed: " << resp.message << "\n";
    }
}

// ─── Monitor Open Trades ──────────────────────────────────────────────────────

void monitorTrades(FyersAPI& api,
                   RiskManager& riskMgr,
                   StrategyEngine& strat,
                   BotState& state,
                   const BotConfig& cfg,
                   Instrument inst) {

    std::vector<std::string> toExit;

    for (auto& [id, trade] : state.openTrades) {
        if (trade.instrument != inst) continue;

        // Get current LTP
        MarketData md = api.getQuote(trade.symbol);
        double currentLTP = md.ltp > 0 ? md.ltp : trade.entryPrice;

        // Update trailing SL
        riskMgr.onMTMUpdate(id, currentLTP);

        // Get current greeks from chain
        OptionGreeks currentGreeks;
        for (auto& e : state.chains[inst]) {
            if (e.symbol == trade.symbol) {
                currentGreeks = e.greeks;
                break;
            }
        }

        // Force exit at no-trade time
        if (isNoTradeTime(cfg)) {
            trade.exitPrice   = currentLTP;
            trade.exitReason  = "TIME_BASED_EXIT";
            trade.exitTime    = nowEpoch();
            trade.realizedPnL = (trade.exitPrice - trade.entryPrice) * trade.qty;
            trade.status      = TradeStatus::CLOSED;
            FyersOrderRequest req;
            req.symbol = trade.symbol; req.side = "-1"; req.qty = trade.qty; req.type = "2";
            api.placeOrder(req);
            riskMgr.onTradeExited(trade);
            toExit.push_back(id);
            continue;
        }

        auto exitDec = strat.checkExit(trade, currentLTP, currentGreeks, state.techs[inst]);
        if (exitDec.shouldExit) {
            trade.exitPrice   = currentLTP;
            trade.exitReason  = exitDec.reason;
            trade.exitTime    = nowEpoch();
            trade.realizedPnL = (trade.exitPrice - trade.entryPrice) * exitDec.exitQty;
            trade.status      = TradeStatus::CLOSED;

            FyersOrderRequest req;
            req.symbol = trade.symbol;
            req.side   = "-1";  // SELL
            req.qty    = exitDec.exitQty;
            req.type   = "2";
            api.placeOrder(req);

            std::cout << "[TRADE] 🚪 EXIT " << trade.symbol
                      << " @" << currentLTP
                      << " PnL=" << trade.realizedPnL
                      << " Reason=" << exitDec.reason << "\n";

            if (exitDec.exitQty >= trade.qty) {
                riskMgr.onTradeExited(trade);
                toExit.push_back(id);
            } else {
                trade.qty -= exitDec.exitQty;
                // Update SL to breakeven after T1
                trade.sl = trade.entryPrice;
            }
        }
    }
    for (auto& id : toExit) state.openTrades.erase(id);
}

// ─── Main Bot Loop ────────────────────────────────────────────────────────────

void runBot(BotConfig& cfg) {
    signal(SIGINT,  sigHandler);
    signal(SIGTERM, sigHandler);

    FyersAPI       api(cfg);
    StrategyEngine strat(cfg);
    RiskManager    riskMgr(cfg.risk);
    BotState       state;

    std::cout << "╔═══════════════════════════════════════════╗\n"
              << "║   NIFTY/FINNIFTY SCALPING BOT v1.0        ║\n"
              << "║   Strategy: RSI + EMA + Option Chain       ║\n"
              << "║   Broker: Fyers  | Mode: "
              << (cfg.paperTrade ? "PAPER TRADE    " : "LIVE TRADE     ")
              << "║\n"
              << "╚═══════════════════════════════════════════╝\n\n";

    if (!api.isAuthenticated()) {
        std::cerr << "[BOT] ERROR: Fyers credentials not set!\n";
        std::cerr << "      Set fyersAppId and fyersAccessToken in config.\n";
        return;
    }

    std::vector<Instrument> instruments;
    if (cfg.tradeNifty)    instruments.push_back(Instrument::NIFTY50);
    if (cfg.tradeFinNifty) instruments.push_back(Instrument::FINNIFTY);

    // ── Main loop (every 60 seconds = 1-min candle close) ────────────────────
    int loopCount = 0;
    while (gRunning) {
        ++loopCount;
        std::cout << "\n[" << nowIST() << "] ── SCAN #" << loopCount << " ──────────────────\n";

        if (!isMarketOpen(cfg)) {
            std::cout << "[BOT] Market closed. Waiting...\n";
            std::this_thread::sleep_for(std::chrono::seconds(30));
            continue;
        }

        if (riskMgr.isKillSwitchActive()) {
            std::cout << "[BOT] Kill switch active. Halting new trades.\n";
            monitorTrades(api, riskMgr, strat, state, cfg, Instrument::NIFTY50);
            monitorTrades(api, riskMgr, strat, state, cfg, Instrument::FINNIFTY);
            std::this_thread::sleep_for(std::chrono::seconds(30));
            continue;
        }

        for (Instrument inst : instruments) {
            std::string spotSymbol = (inst == Instrument::NIFTY50)
                ? api.getNiftySymbol() : api.getFinNiftySymbol();

            // 1. Get spot data
            state.spotData[inst] = api.getQuote(spotSymbol);
            double spot = state.spotData[inst].ltp;
            if (spot < 1.0) {
                std::cerr << "[BOT] Invalid spot for " << spotSymbol << "\n";
                continue;
            }

            // 2. Fetch candles (last 200 1-min, 100 3-min)
            long long now = nowEpoch();
            auto& md = state.spotData[inst];
            md.candles1m = api.getHistoricalData(spotSymbol, "1", now - 200*60, now);
            md.candles3m = api.getHistoricalData(spotSymbol, "3", now - 100*180, now);

            // 3. Compute indicators
            if (!md.candles1m.empty()) {
                state.techs[inst] = IndicatorsEngine::compute(md.candles1m, md.candles3m);
                auto& t = state.techs[inst];
                std::cout << "[TECH] " << spotSymbol
                          << " LTP=" << spot
                          << " RSI14=" << std::fixed << std::setprecision(1) << t.rsi14
                          << " RSI7="  << t.rsi7
                          << " EMA9="  << t.ema9
                          << " EMA20=" << t.ema20
                          << " VWAP="  << t.vwap
                          << (t.emaUp ? " [EMA↑]" : " [EMA↓]")
                          << "\n";
            }

            // 4. Fetch option chain
            std::string expiry = api.getNearestExpiry(spotSymbol);
            state.chains[inst] = api.getOptionChain(spotSymbol, expiry);
            if (!state.chains[inst].empty()) {
                state.flows[inst] = OptionChainAnalyzer::analyze(
                    state.chains[inst], spot);
                auto& f = state.flows[inst];
                std::cout << "[FLOW] PCR=" << std::setprecision(2) << f.pcr
                          << " MaxPain=" << f.maxPainStrike
                          << " Supp=" << f.strongSupportStrike
                          << " Res=" << f.strongResistStrike
                          << " DomFlow=" << f.dominantFlow
                          << " Dir=" << f.direction << "\n";
            }

            // 5. Monitor existing trades first
            monitorTrades(api, riskMgr, strat, state, cfg, inst);

            // 6. Check for new entry signal
            if (!isNoTradeTime(cfg) && !riskMgr.isKillSwitchActive()) {
                auto sig = strat.evaluate(inst,
                                          state.spotData[inst],
                                          state.techs[inst],
                                          state.flows[inst],
                                          state.chains[inst]);

                if (sig.signal != TradeSignal::NONE) {
                    std::cout << "[SIGNAL] " << sig.direction
                              << " signal on " << spotSymbol
                              << " confidence=" << sig.confidenceScore << "%\n";
                    executeTrade(sig, api, riskMgr, state, cfg);
                }
            }
        }

        // Print risk summary every 10 loops
        if (loopCount % 10 == 0) std::cout << riskMgr.getSummary();

        // Sleep until next 1-min bar
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }

    // ── Graceful shutdown ─────────────────────────────────────────────────────
    std::cout << "\n[BOT] Closing all positions...\n";
    for (auto& [id, trade] : state.openTrades) {
        FyersOrderRequest req;
        req.symbol = trade.symbol; req.side = "-1"; req.qty = trade.qty; req.type = "2";
        api.placeOrder(req);
        std::cout << "[BOT] Closed: " << trade.symbol << "\n";
    }
    std::cout << riskMgr.getSummary();
    std::cout << "[BOT] Shutdown complete.\n";
}

// ─── main() ───────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    BotConfig cfg;

    // ── Load from environment or args ─────────────────────────────────────────
    const char* appId    = std::getenv("FYERS_APP_ID");
    const char* token    = std::getenv("FYERS_ACCESS_TOKEN");
    const char* paper    = std::getenv("PAPER_TRADE");

    if (appId) cfg.fyersAppId      = appId;
    if (token) cfg.fyersAccessToken = token;
    cfg.paperTrade = (paper == nullptr || std::string(paper) != "0");

    // ── Instrument config ──────────────────────────────────────────────────────
    cfg.tradeNifty    = true;
    cfg.tradeFinNifty = true;
    cfg.niftyLotSize    = 25;
    cfg.finniftyLotSize = 40;

    // ── Risk config ────────────────────────────────────────────────────────────
    cfg.risk.maxDailyLoss    = -5000.0;
    cfg.risk.maxTradeLoss    = -1500.0;
    cfg.risk.slPct           = 0.30;
    cfg.risk.target1Pct      = 0.40;
    cfg.risk.target2Pct      = 0.80;
    cfg.risk.trailSlPct      = 0.30;
    cfg.risk.maxOpenTrades   = 2;
    cfg.risk.maxTradesPerDay = 10;
    cfg.risk.minDelta        = 0.30;
    cfg.risk.maxDelta        = 0.70;
    cfg.risk.minIV           = 10.0;
    cfg.risk.maxIV           = 60.0;
    cfg.risk.maxTheta        = -8.0;

    runBot(cfg);
    return 0;
}
