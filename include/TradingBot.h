#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <chrono>
#include <atomic>
#include <mutex>

// ─── Core Data Structures ────────────────────────────────────────────────────

struct OptionGreeks {
    double delta   = 0.0;
    double gamma   = 0.0;
    double theta   = 0.0;
    double vega    = 0.0;
    double iv      = 0.0;   // Implied Volatility
    double rho     = 0.0;
};

struct OptionChainEntry {
    std::string symbol;
    int         strikePrice   = 0;
    std::string optionType;   // "CE" or "PE"
    double      ltp           = 0.0;
    double      bid           = 0.0;
    double      ask           = 0.0;
    long long   oi            = 0;      // Open Interest
    long long   oiChange      = 0;
    long long   volume        = 0;
    double      bidQty        = 0;
    double      askQty        = 0;
    OptionGreeks greeks;
    std::string expiry;
};

struct MarketDepth {
    struct Level {
        double price    = 0.0;
        int    qty      = 0;
        int    orders   = 0;
    };
    std::vector<Level> bids;   // top 5 bids
    std::vector<Level> asks;   // top 5 asks
    double totalBidQty = 0.0;
    double totalAskQty = 0.0;
    double bidAskRatio = 0.0;
};

struct Candle {
    long long timestamp = 0;
    double open   = 0.0;
    double high   = 0.0;
    double low    = 0.0;
    double close  = 0.0;
    long long volume = 0;
};

struct MarketData {
    std::string symbol;
    double ltp        = 0.0;
    double open       = 0.0;
    double high       = 0.0;
    double low        = 0.0;
    double prevClose  = 0.0;
    double change     = 0.0;
    double changePct  = 0.0;
    long long volume  = 0;
    MarketDepth depth;
    std::vector<Candle> candles1m;
    std::vector<Candle> candles3m;
    std::vector<Candle> candles5m;
    long long lastUpdate = 0;
};

struct TechnicalIndicators {
    // RSI
    double rsi14     = 0.0;
    double rsi7      = 0.0;
    double rsiPrev   = 0.0;
    bool   rsiOB     = false;   // overbought >70
    bool   rsiOS     = false;   // oversold   <30

    // EMA
    double ema9      = 0.0;
    double ema20     = 0.0;
    double ema50     = 0.0;
    double ema200    = 0.0;

    // VWAP
    double vwap      = 0.0;

    // Derived signals
    bool   emaUp     = false;   // ema9 > ema20
    bool   priceAboveEma = false;
};

struct OptionFlowSignal {
    std::string direction;    // "BULLISH" | "BEARISH" | "NEUTRAL"
    double      pcr        = 0.0;   // Put/Call Ratio (OI based)
    double      pcrVolume  = 0.0;   // PCR by volume
    int         maxPainStrike = 0;
    int         strongSupportStrike = 0;
    int         strongResistStrike  = 0;
    double      ivSkew     = 0.0;   // +ve = bearish skew
    double      callOIChange = 0.0;
    double      putOIChange  = 0.0;
    std::string dominantFlow; // "CALL_WRITING" | "PUT_WRITING" | "CALL_BUYING" | "PUT_BUYING"
};

enum class TradeSignal { NONE, BUY_CE, BUY_PE, EXIT_CE, EXIT_PE };
enum class TradeStatus { OPEN, CLOSED, PENDING };
enum class Instrument  { NIFTY50, FINNIFTY };

struct TradeEntry {
    std::string  id;
    Instrument   instrument;
    std::string  symbol;
    std::string  optionType;   // CE/PE
    int          strikePrice;
    std::string  expiry;
    int          qty           = 0;
    double       entryPrice    = 0.0;
    double       exitPrice     = 0.0;
    double       sl            = 0.0;   // stop-loss
    double       target1       = 0.0;
    double       target2       = 0.0;
    double       mtm           = 0.0;   // mark-to-market P&L
    double       realizedPnL   = 0.0;
    TradeStatus  status        = TradeStatus::PENDING;
    long long    entryTime     = 0;
    long long    exitTime      = 0;
    std::string  exitReason;
    OptionGreeks greeksAtEntry;
};

struct RiskParams {
    double maxDailyLoss    = -5000.0;   // INR
    double maxTradeLoss    = -1500.0;   // per trade
    double trailSlPct      = 0.30;      // 30% trail on profit
    double target1Pct      = 0.40;      // 40% profit → partial exit
    double target2Pct      = 0.80;      // 80% profit → full exit
    double slPct           = 0.30;      // 30% loss on premium
    int    maxOpenTrades   = 2;
    int    maxTradesPerDay = 10;
    // Greeks filters
    double minDelta        = 0.30;
    double maxDelta        = 0.70;
    double minIV           = 10.0;
    double maxIV           = 60.0;
    double maxTheta        = -5.0;      // don't hold options with theta < -5
};

struct BotConfig {
    // Fyers credentials
    std::string fyersAppId;
    std::string fyersAccessToken;
    std::string fyersApiBase = "https://api-t1.fyers.in/api/v3";
    std::string fyersDataApi = "https://api-t1.fyers.in/data-rest/v2";

    // Instruments
    bool tradeNifty   = true;
    bool tradeFinNifty = true;

    // Lot sizes
    int niftyLotSize    = 25;
    int finniftyLotSize = 40;

    // Scalping timeframe
    std::string primaryTF  = "1";   // 1-min candles
    std::string confirmTF  = "3";   // 3-min confirmation

    // Market hours (IST)
    std::string marketOpen  = "09:15";
    std::string marketClose = "15:30";
    std::string noTradeStart = "15:15";   // stop fresh entries after this

    RiskParams risk;
    bool   paperTrade  = true;   // simulation mode
    bool   verbose     = true;
};
