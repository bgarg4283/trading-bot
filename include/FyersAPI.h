#pragma once
#include "TradingBot.h"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ─── Fyers API Response Wrappers ─────────────────────────────────────────────

struct FyersOrderRequest {
    std::string symbol;
    std::string type      = "2";    // 1=Limit, 2=Market
    std::string side      = "1";    // 1=Buy, -1=Sell
    int         qty       = 0;
    double      limitPrice = 0.0;
    std::string productType = "INTRADAY"; // INTRADAY / MARGIN / CNC
    std::string validity    = "DAY";
    std::string offlineOrder = "False";
    std::string orderTag    = "SCALPBOT";
};

struct FyersOrderResponse {
    bool        success  = false;
    std::string orderId;
    std::string message;
    int         code     = 0;
};

struct FyersPosition {
    std::string symbol;
    int         qty         = 0;
    double      avgPrice    = 0.0;
    double      ltp         = 0.0;
    double      pnl         = 0.0;
    std::string side;
    std::string productType;
};

// ─── FyersAPI Class ──────────────────────────────────────────────────────────

class FyersAPI {
public:
    explicit FyersAPI(const BotConfig& cfg);

    // Auth
    bool isAuthenticated() const;
    std::string getProfile();

    // Market Data
    MarketData      getQuote(const std::string& symbol);
    std::vector<OptionChainEntry> getOptionChain(const std::string& underlying,
                                                  const std::string& expiry);
    std::vector<Candle> getHistoricalData(const std::string& symbol,
                                           const std::string& resolution,
                                           long long fromEpoch,
                                           long long toEpoch);

    // Orders
    FyersOrderResponse placeOrder(const FyersOrderRequest& req);
    FyersOrderResponse cancelOrder(const std::string& orderId);
    FyersOrderResponse modifyOrder(const std::string& orderId, double newPrice, int newQty = 0);

    // Portfolio
    std::vector<FyersPosition> getPositions();
    double                     getAvailableMargin();

    // Helpers
    std::string getNiftySymbol()    const { return "NSE:NIFTY50-INDEX"; }
    std::string getFinNiftySymbol() const { return "NSE:FINNIFTY-INDEX"; }
    std::string buildOptionSymbol(const std::string& underlying,
                                   const std::string& expiry,
                                   int strike,
                                   const std::string& type) const;
    std::string getNearestExpiry(const std::string& underlying) const;

private:
    BotConfig cfg_;
    std::string baseHeaders_;

    json  httpGet (const std::string& endpoint, const std::string& params = "");
    json  httpPost(const std::string& endpoint, const json& body);

    // Parse helpers
    MarketData        parseQuote(const json& j);
    OptionChainEntry  parseOptionEntry(const json& j);
    Candle            parseCandle(const json& j);
};
