#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <chrono>
#include <atomic>
#include <mutex>

struct OptionGreeks {
    double delta = 0.0;
    double gamma = 0.0;
    double theta = 0.0;
    double vega  = 0.0;
    double iv    = 0.0;
    double rho   = 0.0;
};

struct OptionChainEntry {
    std::string symbol;
    int         strikePrice = 0;
    std::string optionType;
    double      ltp         = 0.0;
    double      bid         = 0.0;
    double      ask         = 0.0;
    long long   oi          = 0;
    long long   oiChange    = 0;
    long long   volume      = 0;
    double      bidQty      = 0;
    double      askQty      = 0;
    OptionGreeks greeks;
    std::string expiry;
};

struct MarketDepth {
    struct Level {
        double price  = 0.0;
        int    qty    = 0;
        int    orders = 0;
    };
    std::vector<Level> bids;
    std::vector<Level> asks;
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
    double    ltp       = 0.0;
    double    open      = 0.0;
    double    high      = 0.0;
    double    low       = 0.0;
    double    prevClose = 0.0;
    double    change    = 0.0;
    double    changePct = 0.0;
    long long volume    = 0;
    MarketDepth depth;
    std::vector<Candle> candles1m;
    std::vector<Candle> candles3m;
    std::vector<Candle> candles5m;
    long long lastUpdate = 0;
};

struct TechnicalIndicators {
    double rsi14         = 0.0;
    double rsi7          = 0.0;
    double rsiPrev       = 0.0;
    bool   rsiOB         = false;
    bool   rsiOS         = false;
    double ema9          = 0.0;
    double ema20         = 0.0;
    double ema50         = 0.0;
    double ema200        = 0.0;
    double vwap          = 0.0;
    bool   emaUp         = false;
    bool   priceAboveEma = false;
};

struct OptionFlowSignal {
    std::string direction;
    double      pcr              = 0.0;
    double      pcrVolume        = 0.0;
    int         maxPainStrike    = 0;
    int         strongSupportStrike = 0;
    int         strongResistStrike  = 0;
    double      ivSkew           = 0.0;
    double      callOIChange     = 0.0;
    double      putOIChange      = 0.0;
    std::string dominantFlow;
};

enum class TradeSignal { NONE, BUY_CE, BUY_PE, EXIT_CE, EXIT_PE };
enum class TradeStatus { OPEN, CLOSED, PENDING };
enum class Instrument  { NIFTY50, FINNIFTY };

struct TradeEntry {
    std::string  id;
    Instrument   instrument   = Instrument::NIFTY50;
    std::string  symbol;
    std::string  optionType;
    int          strikePrice  = 0;
    std::string  expiry;
    int          qty          = 0;
    double       entryPrice   = 0.0;
    double       exitPrice    = 0.0;
    double       sl           = 0.0;
    double       target1      = 0.0;
    double       target2      = 0.0;
    double       mtm          = 0.0;
    double       realizedPnL  = 0.0;
    TradeStatus  status       = TradeStatus::PENDING;
    long long    entryTime    = 0;
    long long    exitTime     = 0;
    std::string  exitReason;
    OptionGreeks greeksAtEntry;
};

struct RiskParams {
    double maxDailyLoss    = -5000.0;
    double maxTradeLoss    = -1500.0;
    double trailSlPct      = 0.30;
    double target1Pct      = 0.40;
    double target2Pct      = 0.80;
    double slPct           = 0.30;
    int    maxOpenTrades   = 2;
    int    maxTradesPerDay = 10;
    double minDelta        = 0.30;
    double maxDelta        = 0.70;
    double minIV           = 10.0;
    double maxIV           = 60.0;
    double maxTheta        = -5.0;
};

struct BotConfig {
    std::string fyersAppId;
    std::string fyersAccessToken;
    std::string fyersApiBase  = "https://api-t1.fyers.in/api/v3";
    std::string fyersDataApi  = "https://api-t1.fyers.in/data-rest/v2";
    bool tradeNifty           = true;
    bool tradeFinNifty        = true;
    int  niftyLotSize         = 25;
    int  finniftyLotSize      = 40;
    std::string primaryTF     = "1";
    std::string confirmTF     = "3";
    std::string marketOpen    = "09:15";
    std::string marketClose   = "15:30";
    std::string noTradeStart  = "15:15";
    RiskParams risk;
    bool paperTrade           = true;
    bool verbose              = true;
};
