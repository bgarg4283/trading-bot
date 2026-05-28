#include "FyersAPI.h"
#include <curl/curl.h>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <ctime>
#include <stdexcept>

// ─── CURL write callback ──────────────────────────────────────────────────────

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    s->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// ─── Constructor ──────────────────────────────────────────────────────────────

FyersAPI::FyersAPI(const BotConfig& cfg) : cfg_(cfg) {
    curl_global_init(CURL_GLOBAL_ALL);
}

bool FyersAPI::isAuthenticated() const {
    return !cfg_.fyersAccessToken.empty() && !cfg_.fyersAppId.empty();
}

// ─── HTTP helpers ─────────────────────────────────────────────────────────────

json FyersAPI::httpGet(const std::string& endpoint, const std::string& params) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("CURL init failed");

    std::string url = cfg_.fyersApiBase + endpoint;
    if (!params.empty()) url += "?" + params;

    std::string response;
    struct curl_slist* headers = nullptr;
    std::string authHeader = "Authorization: " + cfg_.fyersAppId + ":" + cfg_.fyersAccessToken;
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        throw std::runtime_error(std::string("CURL error: ") + curl_easy_strerror(res));

    return json::parse(response);
}

json FyersAPI::httpPost(const std::string& endpoint, const json& body) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("CURL init failed");

    std::string url = cfg_.fyersApiBase + endpoint;
    std::string postData = body.dump();
    std::string response;

    struct curl_slist* headers = nullptr;
    std::string authHeader = "Authorization: " + cfg_.fyersAppId + ":" + cfg_.fyersAccessToken;
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        throw std::runtime_error(std::string("CURL error: ") + curl_easy_strerror(res));

    return json::parse(response);
}

// ─── Market Data ──────────────────────────────────────────────────────────────

MarketData FyersAPI::getQuote(const std::string& symbol) {
    try {
        auto j = httpGet("/quotes", "symbols=" + symbol);
        if (j.contains("d") && j["d"].is_array() && !j["d"].empty())
            return parseQuote(j["d"][0]["v"]);
    } catch (std::exception& e) {
        std::cerr << "[FyersAPI] getQuote error: " << e.what() << "\n";
    }
    return {};
}

MarketData FyersAPI::parseQuote(const json& v) {
    MarketData md;
    md.ltp      = v.value("lp",  0.0);
    md.open     = v.value("o",   0.0);
    md.high     = v.value("h",   0.0);
    md.low      = v.value("l",   0.0);
    md.prevClose= v.value("prev_close_price", 0.0);
    md.change   = v.value("ch",  0.0);
    md.changePct= v.value("chp", 0.0);
    md.volume   = v.value("vol", 0LL);
    md.symbol   = v.value("symbol", "");

    // Depth
    if (v.contains("bids") && v["bids"].is_array()) {
        for (auto& b : v["bids"]) {
            MarketDepth::Level lvl;
            lvl.price = b.value("price", 0.0);
            lvl.qty   = b.value("volume", 0);
            md.depth.bids.push_back(lvl);
            md.depth.totalBidQty += lvl.qty;
        }
    }
    if (v.contains("asks") && v["asks"].is_array()) {
        for (auto& a : v["asks"]) {
            MarketDepth::Level lvl;
            lvl.price = a.value("price", 0.0);
            lvl.qty   = a.value("volume", 0);
            md.depth.asks.push_back(lvl);
            md.depth.totalAskQty += lvl.qty;
        }
    }
    if (md.depth.totalAskQty > 0)
        md.depth.bidAskRatio = md.depth.totalBidQty / md.depth.totalAskQty;

    return md;
}

// ─── Historical Data ──────────────────────────────────────────────────────────

std::vector<Candle> FyersAPI::getHistoricalData(const std::string& symbol,
                                                  const std::string& resolution,
                                                  long long fromEpoch,
                                                  long long toEpoch) {
    std::ostringstream params;
    params << "symbol=" << symbol
           << "&resolution=" << resolution
           << "&date_format=0"
           << "&range_from=" << fromEpoch
           << "&range_to="   << toEpoch
           << "&cont_flag=1";

    std::vector<Candle> candles;
    try {
        // Fyers historical data uses data API
        CURL* curl = curl_easy_init();
        if (!curl) return candles;

        std::string url = cfg_.fyersDataApi + "/history?" + params.str();
        std::string response;
        struct curl_slist* headers = nullptr;
        std::string authHeader = "Authorization: " + cfg_.fyersAppId + ":" + cfg_.fyersAccessToken;
        headers = curl_slist_append(headers, authHeader.c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
        curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        auto j = json::parse(response);
        if (j.contains("candles") && j["candles"].is_array()) {
            for (auto& c : j["candles"]) {
                Candle candle;
                candle.timestamp = c[0];
                candle.open      = c[1];
                candle.high      = c[2];
                candle.low       = c[3];
                candle.close     = c[4];
                candle.volume    = c[5];
                candles.push_back(candle);
            }
        }
    } catch (std::exception& e) {
        std::cerr << "[FyersAPI] getHistoricalData error: " << e.what() << "\n";
    }
    return candles;
}

// ─── Option Chain ─────────────────────────────────────────────────────────────

std::vector<OptionChainEntry> FyersAPI::getOptionChain(const std::string& underlying,
                                                         const std::string& expiry) {
    std::vector<OptionChainEntry> entries;
    try {
        std::string params = "symbol=" + underlying + "&strikecount=20&timestamp=" + expiry;
        auto j = httpGet("/options/chain", params);

        if (!j.contains("data")) return entries;
        auto& data = j["data"];

        auto parseEntries = [&](const json& arr, const std::string& type) {
            for (auto& item : arr) {
                OptionChainEntry e;
                e.optionType  = type;
                e.strikePrice = item.value("strike_price", 0);
                e.expiry      = expiry;
                e.ltp         = item.value("ltp", 0.0);
                e.bid         = item.value("bid", 0.0);
                e.ask         = item.value("ask", 0.0);
                e.oi          = item.value("oi", 0LL);
                e.oiChange    = item.value("oi_change", 0LL);
                e.volume      = item.value("volume", 0LL);
                e.bidQty      = item.value("bid_qty", 0.0);
                e.askQty      = item.value("ask_qty", 0.0);

                // Greeks
                if (item.contains("greeks")) {
                    auto& g = item["greeks"];
                    e.greeks.delta = g.value("delta", 0.0);
                    e.greeks.gamma = g.value("gamma", 0.0);
                    e.greeks.theta = g.value("theta", 0.0);
                    e.greeks.vega  = g.value("vega",  0.0);
                    e.greeks.iv    = g.value("iv",    0.0);
                }

                // Build symbol: e.g. NSE:NIFTY2560522000CE
                e.symbol = buildOptionSymbol(underlying, expiry, e.strikePrice, type);
                entries.push_back(e);
            }
        };

        if (data.contains("CE")) parseEntries(data["CE"], "CE");
        if (data.contains("PE")) parseEntries(data["PE"], "PE");

    } catch (std::exception& e) {
        std::cerr << "[FyersAPI] getOptionChain error: " << e.what() << "\n";
    }
    return entries;
}

// ─── Orders ───────────────────────────────────────────────────────────────────

FyersOrderResponse FyersAPI::placeOrder(const FyersOrderRequest& req) {
    FyersOrderResponse resp;
    if (cfg_.paperTrade) {
        resp.success = true;
        resp.orderId = "PAPER_" + std::to_string(std::time(nullptr));
        resp.message = "[PAPER TRADE] Order simulated";
        std::cout << "[ORDER] PAPER TRADE: " << req.side << " " << req.qty
                  << " " << req.symbol << " @ MARKET\n";
        return resp;
    }
    try {
        json body = {
            {"symbol",      req.symbol},
            {"qty",         req.qty},
            {"type",        std::stoi(req.type)},
            {"side",        std::stoi(req.side)},
            {"productType", req.productType},
            {"limitPrice",  req.limitPrice},
            {"stopPrice",   0},
            {"validity",    req.validity},
            {"disclosedQty",0},
            {"offlineOrder",req.offlineOrder},
            {"orderTag",    req.orderTag}
        };
        auto j = httpPost("/orders", body);
        resp.success  = (j.value("s", "") == "ok");
        resp.orderId  = j.value("id", "");
        resp.message  = j.value("message", "");
        resp.code     = j.value("code", 0);
    } catch (std::exception& e) {
        resp.message = e.what();
    }
    return resp;
}

FyersOrderResponse FyersAPI::cancelOrder(const std::string& orderId) {
    FyersOrderResponse resp;
    try {
        json body = {{"id", orderId}};
        auto j = httpPost("/orders/cancel", body);
        resp.success = (j.value("s", "") == "ok");
        resp.message = j.value("message", "");
    } catch (std::exception& e) {
        resp.message = e.what();
    }
    return resp;
}

// ─── Positions ────────────────────────────────────────────────────────────────

std::vector<FyersPosition> FyersAPI::getPositions() {
    std::vector<FyersPosition> positions;
    try {
        auto j = httpGet("/positions");
        if (j.contains("netPositions") && j["netPositions"].is_array()) {
            for (auto& p : j["netPositions"]) {
                FyersPosition pos;
                pos.symbol      = p.value("symbol", "");
                pos.qty         = p.value("netQty", 0);
                pos.avgPrice    = p.value("avgPrice", 0.0);
                pos.ltp         = p.value("ltp", 0.0);
                pos.pnl         = p.value("pl", 0.0);
                pos.productType = p.value("productType", "");
                positions.push_back(pos);
            }
        }
    } catch (std::exception& e) {
        std::cerr << "[FyersAPI] getPositions error: " << e.what() << "\n";
    }
    return positions;
}

double FyersAPI::getAvailableMargin() {
    try {
        auto j = httpGet("/funds");
        if (j.contains("fund_limit") && j["fund_limit"].is_array()) {
            for (auto& f : j["fund_limit"]) {
                if (f.value("title", "") == "Available Balance")
                    return f.value("equityAmount", 0.0);
            }
        }
    } catch (...) {}
    return 0.0;
}

// ─── Symbol helpers ───────────────────────────────────────────────────────────

std::string FyersAPI::buildOptionSymbol(const std::string& underlying,
                                         const std::string& expiry,
                                         int strike,
                                         const std::string& type) const {
    // Fyers format: NSE:NIFTY25605XXXCE  (YYMMM format)
    // underlying: NSE:NIFTY50-INDEX → NIFTY
    std::string base = "NIFTY";
    if (underlying.find("FINNIFTY") != std::string::npos) base = "FINNIFTY";

    // Parse expiry to YYMMM: "2025-06-05" → "25JUN"
    std::string expCode = expiry.substr(2, 2) + expiry.substr(5, 2) + expiry.substr(8, 2);
    // For weekly: YYMMDD format in Fyers
    return "NSE:" + base + expCode + std::to_string(strike) + type;
}

std::string FyersAPI::getNearestExpiry(const std::string& underlying) const {
    // Returns current week's Thursday expiry in YYYY-MM-DD format
    // Simplified: return hardcoded placeholder; real impl would calc next Thursday
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    // Find next Thursday (weekday 4)
    int daysToThursday = (4 - t->tm_wday + 7) % 7;
    if (daysToThursday == 0) daysToThursday = 0;  // today is Thursday
    t->tm_mday += daysToThursday;
    mktime(t);
    char buf[12];
    strftime(buf, sizeof(buf), "%Y-%m-%d", t);
    return std::string(buf);
}

std::string FyersAPI::getProfile() {
    try {
        auto j = httpGet("/profile");
        return j.dump(2);
    } catch (std::exception& e) {
        return std::string("Error: ") + e.what();
    }
}
