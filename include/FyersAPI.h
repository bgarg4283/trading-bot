#pragma once
#include "TradingBot.h"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct FyersOrderRequest {
    std::string symbol;
    std::string type        = "2";
    std::string side        = "1";
    int         qty         = 0;
    double      limitPrice  = 0.0;
    std::string productType = "INTRADAY";
    std::string validity    = "DAY";
    std::string offlineOrder= "False";
    std::string orderTag    = "SCALPBOT";
};

struct FyersOrderResponse {
    bool        success = false;
    std::string orderId;
    std::string message;
    int         code    = 0;
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

class FyersAPI {
public:
    explicit FyersAPI(const BotConfig& cfg);

    bool        isAuthenticated() const;
    std::string getProfile();

    MarketData getQuote(const std::string& symbol);
    std::vector<OptionChainEntry> getOptionChain(const std::string& underlying,
                                                  const std::string& expiry);
    std::vector<Candle> getHistoricalData(const std::string& symbol,
                                           const std::string& resolution,
                                           long long fromEpoch,
                                           long long toEpoch);

    FyersOrderResponse placeOrder (const FyersOrderRequest& req);
    FyersOrderResponse cancelOrder(const std::string& orderId);

    std::vector<FyersPosition> getPositions();
    double                     getAvailableMargin();

    std::string getNiftySymbol()    const { return "NSE:NIFTY50-INDEX"; }
    std::string getFinNiftySymbol() const { return "NSE:FINNIFTY-INDEX"; }
    std::string buildOptionSymbol(const std::string& underlying,
                                   const std::string& expiry,
                                   int strike,
                                   const std::string& type) const;
    std::string getNearestExpiry() const;

private:
    BotConfig   cfg_;
    json        httpGet (const std::string& endpoint, const std::string& params = "");
    json        httpPost(const std::string& endpoint, const json& body);
    MarketData       parseQuote(const json& j);
    OptionChainEntry parseOptionEntry(const json& j);
};
