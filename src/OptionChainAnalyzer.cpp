#include "OptionChainAnalyzer.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <map>
#include <limits>

// ─── PCR ──────────────────────────────────────────────────────────────────────

double OptionChainAnalyzer::calcPCR_OI(const std::vector<OptionChainEntry>& chain) {
    long long callOI = 0, putOI = 0;
    for (auto& e : chain) {
        if (e.optionType == "CE") callOI += e.oi;
        else                      putOI  += e.oi;
    }
    return (callOI > 0) ? (double)putOI / callOI : 1.0;
}

double OptionChainAnalyzer::calcPCR_Volume(const std::vector<OptionChainEntry>& chain) {
    long long callVol = 0, putVol = 0;
    for (auto& e : chain) {
        if (e.optionType == "CE") callVol += e.volume;
        else                      putVol  += e.volume;
    }
    return (callVol > 0) ? (double)putVol / callVol : 1.0;
}

// ─── Max Pain ─────────────────────────────────────────────────────────────────

int OptionChainAnalyzer::calcMaxPain(const std::vector<OptionChainEntry>& chain) {
    // collect unique strikes
    std::vector<int> strikes;
    for (auto& e : chain) {
        if (std::find(strikes.begin(), strikes.end(), e.strikePrice) == strikes.end())
            strikes.push_back(e.strikePrice);
    }
    std::sort(strikes.begin(), strikes.end());

    int    maxPainStrike = strikes.empty() ? 0 : strikes[strikes.size()/2];
    double minLoss = std::numeric_limits<double>::max();

    for (int testStrike : strikes) {
        double totalLoss = 0.0;
        for (auto& e : chain) {
            if (e.optionType == "CE") {
                // Call buyer loses when test > strike
                if (testStrike > e.strikePrice)
                    totalLoss += (double)(testStrike - e.strikePrice) * e.oi;
            } else {
                // Put buyer loses when test < strike
                if (testStrike < e.strikePrice)
                    totalLoss += (double)(e.strikePrice - testStrike) * e.oi;
            }
        }
        if (totalLoss < minLoss) {
            minLoss = totalLoss;
            maxPainStrike = testStrike;
        }
    }
    return maxPainStrike;
}

// ─── ATM ─────────────────────────────────────────────────────────────────────

int OptionChainAnalyzer::getATMStrike(const std::vector<OptionChainEntry>& chain, double spot) {
    int atm = 0;
    double minDiff = std::numeric_limits<double>::max();
    for (auto& e : chain) {
        double d = std::abs(e.strikePrice - spot);
        if (d < minDiff) { minDiff = d; atm = e.strikePrice; }
    }
    return atm;
}

// ─── Max OI strikes ───────────────────────────────────────────────────────────

int OptionChainAnalyzer::getMaxOICallStrike(const std::vector<OptionChainEntry>& chain) {
    int strike = 0; long long maxOI = 0;
    for (auto& e : chain)
        if (e.optionType == "CE" && e.oi > maxOI) { maxOI = e.oi; strike = e.strikePrice; }
    return strike;
}

int OptionChainAnalyzer::getMaxOIPutStrike(const std::vector<OptionChainEntry>& chain) {
    int strike = 0; long long maxOI = 0;
    for (auto& e : chain)
        if (e.optionType == "PE" && e.oi > maxOI) { maxOI = e.oi; strike = e.strikePrice; }
    return strike;
}

// ─── IV Skew ──────────────────────────────────────────────────────────────────

double OptionChainAnalyzer::calcIVSkew(const std::vector<OptionChainEntry>& chain, double spot) {
    // OTM puts: strike < spot-1%, OTM calls: strike > spot+1%
    double sumPutIV = 0, sumCallIV = 0;
    int putCount = 0, callCount = 0;
    for (auto& e : chain) {
        if (e.optionType == "PE" && e.strikePrice < spot * 0.99) {
            sumPutIV += e.greeks.iv; ++putCount;
        }
        if (e.optionType == "CE" && e.strikePrice > spot * 1.01) {
            sumCallIV += e.greeks.iv; ++callCount;
        }
    }
    double avgPut  = (putCount  > 0) ? sumPutIV  / putCount  : 0.0;
    double avgCall = (callCount > 0) ? sumCallIV / callCount : 0.0;
    return avgPut - avgCall;   // +ve → put skew → bearish sentiment
}

// ─── Flow Classification ──────────────────────────────────────────────────────

std::string OptionChainAnalyzer::classifyFlow(const std::vector<OptionChainEntry>& chain,
                                               double spot, int atmRange) {
    int atm = getATMStrike(chain, spot);
    long long callOIBuild = 0, putOIBuild = 0;
    long long callOIDrop  = 0, putOIDrop  = 0;

    for (auto& e : chain) {
        bool nearATM = (std::abs(e.strikePrice - atm) <= atmRange * 50);
        if (!nearATM) continue;
        if (e.optionType == "CE") {
            if (e.oiChange > 0) callOIBuild += e.oiChange;
            else                callOIDrop  -= e.oiChange;
        } else {
            if (e.oiChange > 0) putOIBuild += e.oiChange;
            else                putOIDrop  -= e.oiChange;
        }
    }

    // Dominant signal
    long long maxVal = std::max({callOIBuild, putOIBuild, callOIDrop, putOIDrop});
    if (maxVal == callOIBuild) return "CALL_WRITING";
    if (maxVal == putOIBuild)  return "PUT_WRITING";
    if (maxVal == callOIDrop)  return "CALL_UNWINDING";
    return "PUT_UNWINDING";
}

// ─── Flow Score ───────────────────────────────────────────────────────────────

double OptionChainAnalyzer::getFlowScore(const OptionFlowSignal& s) {
    double score = 0.0;
    // PCR: lower PCR = more bullish (more calls being written)
    if      (s.pcr < 0.7)  score += 30;
    else if (s.pcr < 0.9)  score += 15;
    else if (s.pcr > 1.3)  score -= 30;
    else if (s.pcr > 1.1)  score -= 15;

    if (s.dominantFlow == "PUT_WRITING")   score += 25;
    if (s.dominantFlow == "CALL_WRITING")  score -= 25;
    if (s.dominantFlow == "CALL_UNWINDING")score += 10;
    if (s.dominantFlow == "PUT_UNWINDING") score -= 10;

    if (s.ivSkew > 2)  score -= 10;  // bearish skew
    if (s.ivSkew < -2) score += 10;  // bullish skew
    return score;
}

// ─── Delta estimation (simplified Black-Scholes N(d1)) ────────────────────────

static double normCDF(double x) {
    return 0.5 * std::erfc(-x / std::sqrt(2.0));
}

double OptionChainAnalyzer::estimateDelta(double spot, int strike, double iv,
                                           double dte, const std::string& optType) {
    if (iv <= 0 || dte <= 0) return 0.5;
    double r    = 0.065;   // risk-free rate ~6.5%
    double T    = dte / 365.0;
    double sig  = iv / 100.0;
    double d1   = (std::log((double)spot / strike) + (r + 0.5*sig*sig)*T) / (sig * std::sqrt(T));
    double delta = normCDF(d1);
    if (optType == "PE") delta -= 1.0;
    return delta;
}

// ─── Best Strike Selection ────────────────────────────────────────────────────

OptionChainEntry OptionChainAnalyzer::selectBestStrike(
        const std::vector<OptionChainEntry>& chain,
        double spot,
        const std::string& direction,
        const RiskParams& risk) {

    OptionChainEntry best;
    double bestScore = -1e9;

    for (auto& e : chain) {
        if (e.optionType != direction) continue;
        if (!greeksOK(e.greeks, risk, direction)) continue;

        // Spread check: bid-ask < 2%
        if (e.ask > 0 && e.bid > 0) {
            double spread = (e.ask - e.bid) / e.ltp;
            if (spread > 0.02) continue;
        }

        // Liquidity: min volume
        if (e.volume < 100) continue;

        // Score: prefer ATM, good delta, lower theta cost
        double moneyness = std::abs(e.strikePrice - spot) / spot;
        double score     = -moneyness * 100          // closer to ATM is better
                         + e.greeks.delta * 50       // higher delta (CE) is better
                         - std::abs(e.greeks.theta)  // lower theta cost is better
                         + e.volume / 10000.0;       // more liquid is better

        if (score > bestScore) {
            bestScore = score;
            best = e;
        }
    }
    return best;
}

bool OptionChainAnalyzer::greeksOK(const OptionGreeks& g, const RiskParams& risk,
                                    const std::string& optType) {
    double absDelta = std::abs(g.delta);
    if (absDelta < risk.minDelta || absDelta > risk.maxDelta) return false;
    if (g.iv < risk.minIV || g.iv > risk.maxIV) return false;
    if (g.theta < risk.maxTheta) return false;
    return true;
}

double OptionChainAnalyzer::bidAskImbalance(const OptionChainEntry& e) {
    double total = e.bidQty + e.askQty;
    if (total == 0) return 0.0;
    return (e.bidQty - e.askQty) / total;  // +1 = all bids, -1 = all asks
}

double OptionChainAnalyzer::chainImbalanceScore(const std::vector<OptionChainEntry>& chain,
                                                 double spot, int atmRange) {
    int atm = getATMStrike(chain, spot);
    double score = 0.0;
    for (auto& e : chain) {
        if (std::abs(e.strikePrice - atm) > atmRange * 50) continue;
        score += bidAskImbalance(e) * e.oi;
    }
    return score;
}

// ─── Main analyze ─────────────────────────────────────────────────────────────

OptionFlowSignal OptionChainAnalyzer::analyze(const std::vector<OptionChainEntry>& chain,
                                               double spot, int atmRange) {
    OptionFlowSignal sig;
    sig.pcr             = calcPCR_OI(chain);
    sig.pcrVolume       = calcPCR_Volume(chain);
    sig.maxPainStrike   = calcMaxPain(chain);
    sig.strongSupportStrike = getMaxOIPutStrike(chain);
    sig.strongResistStrike  = getMaxOICallStrike(chain);
    sig.ivSkew          = calcIVSkew(chain, spot);
    sig.dominantFlow    = classifyFlow(chain, spot, atmRange);

    // OI changes
    long long callOI = 0, putOI = 0, callOIChg = 0, putOIChg = 0;
    for (auto& e : chain) {
        if (e.optionType == "CE") { callOI += e.oi; callOIChg += e.oiChange; }
        else                      { putOI  += e.oi; putOIChg  += e.oiChange; }
    }
    sig.callOIChange = (double)callOIChg;
    sig.putOIChange  = (double)putOIChg;

    // Direction
    double score = getFlowScore(sig);
    if      (score >  20) sig.direction = "BULLISH";
    else if (score < -20) sig.direction = "BEARISH";
    else                  sig.direction = "NEUTRAL";

    return sig;
}
