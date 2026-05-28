#include "StrategyEngine.h"
#include <cmath>
#include <sstream>
#include <iostream>

StrategyEngine::StrategyEngine(const BotConfig& cfg)
    : cfg_(cfg), risk_(cfg.risk) {}

// ─── Main Evaluate ────────────────────────────────────────────────────────────

StrategySignal StrategyEngine::evaluate(Instrument inst,
                                         const MarketData& spot,
                                         const TechnicalIndicators& tech,
                                         const OptionFlowSignal& flow,
                                         const std::vector<OptionChainEntry>& chain) {
    StrategySignal sig;
    sig.instrument = inst;

    // ── Try CE entry ──────────────────────────────────────────────────────────
    if (checkCEEntry(spot, tech, flow, sig)) {
        sig.signal    = TradeSignal::BUY_CE;
        sig.direction = "CE";
        sig.selectedOption = OptionChainAnalyzer::selectBestStrike(chain, spot.ltp, "CE", risk_);
        sig.confidenceScore = scoreSignal(sig, flow);
        std::ostringstream r;
        r << "CE: RSI14=" << tech.rsi14 << " RSI7=" << tech.rsi7
          << " EMA9>EMA20=" << tech.emaUp
          << " VWAP=" << tech.vwap << " PCR=" << flow.pcr
          << " Flow=" << flow.dominantFlow;
        sig.reason = r.str();
        return sig;
    }

    // ── Try PE entry ──────────────────────────────────────────────────────────
    if (checkPEEntry(spot, tech, flow, sig)) {
        sig.signal    = TradeSignal::BUY_PE;
        sig.direction = "PE";
        sig.selectedOption = OptionChainAnalyzer::selectBestStrike(chain, spot.ltp, "PE", risk_);
        sig.confidenceScore = scoreSignal(sig, flow);
        std::ostringstream r;
        r << "PE: RSI14=" << tech.rsi14 << " RSI7=" << tech.rsi7
          << " EMA9<EMA20=" << !tech.emaUp
          << " VWAP=" << tech.vwap << " PCR=" << flow.pcr
          << " Flow=" << flow.dominantFlow;
        sig.reason = r.str();
        return sig;
    }

    return sig;
}

// ─── CE Entry Conditions ──────────────────────────────────────────────────────

bool StrategyEngine::checkCEEntry(const MarketData& spot,
                                   const TechnicalIndicators& tech,
                                   const OptionFlowSignal& flow,
                                   StrategySignal& out) const {
    // 1. RSI: either crossed above 40 (recovery) OR RSI7 > 55 (momentum)
    bool rsiCross = IndicatorsEngine::rsiCrossedAbove(tech.rsi14, tech.rsiPrev, 40.0);
    bool rsiMom   = (tech.rsi7 > 55.0 && tech.rsi14 > 50.0);
    bool rsiNotOB = (tech.rsi14 < 72.0);  // avoid chasing overbought
    out.rsiOK = (rsiCross || rsiMom) && rsiNotOB;

    // 2. EMA: 9-EMA above 20-EMA (short-term uptrend)
    out.emaOK = tech.emaUp;

    // 3. Price above VWAP
    out.vwapOK = (spot.ltp > tech.vwap);

    // 4. Option flow bullish
    bool pcrBullish = (flow.pcr < 0.90);
    bool flowBullish = (flow.dominantFlow == "PUT_WRITING" ||
                        flow.dominantFlow == "CALL_UNWINDING");
    bool dirBullish  = (flow.direction == "BULLISH");
    out.flowOK = (pcrBullish || flowBullish || dirBullish);

    // 5. No strong call wall immediately above (resistance)
    bool noNearResist = (flow.strongResistStrike == 0 ||
                         flow.strongResistStrike > spot.ltp * 1.005);
    out.greeksOK = noNearResist;

    return out.rsiOK && out.emaOK && out.vwapOK && out.flowOK && out.greeksOK;
}

// ─── PE Entry Conditions ──────────────────────────────────────────────────────

bool StrategyEngine::checkPEEntry(const MarketData& spot,
                                   const TechnicalIndicators& tech,
                                   const OptionFlowSignal& flow,
                                   StrategySignal& out) const {
    // 1. RSI: crossed below 60 (reversal) OR RSI7 < 45 (bearish momentum)
    bool rsiCross = IndicatorsEngine::rsiCrossedBelow(tech.rsi14, tech.rsiPrev, 60.0);
    bool rsiMom   = (tech.rsi7 < 45.0 && tech.rsi14 < 50.0);
    bool rsiNotOS = (tech.rsi14 > 28.0);  // avoid chasing oversold
    out.rsiOK = (rsiCross || rsiMom) && rsiNotOS;

    // 2. EMA: 9-EMA below 20-EMA
    out.emaOK = !tech.emaUp;

    // 3. Price below VWAP
    out.vwapOK = (spot.ltp < tech.vwap);

    // 4. Option flow bearish
    bool pcrBearish = (flow.pcr > 1.10);
    bool flowBearish = (flow.dominantFlow == "CALL_WRITING" ||
                        flow.dominantFlow == "PUT_UNWINDING");
    bool dirBearish  = (flow.direction == "BEARISH");
    out.flowOK = (pcrBearish || flowBearish || dirBearish);

    // 5. No strong put support immediately below
    bool noNearSupport = (flow.strongSupportStrike == 0 ||
                          flow.strongSupportStrike < spot.ltp * 0.995);
    out.greeksOK = noNearSupport;

    return out.rsiOK && out.emaOK && out.vwapOK && out.flowOK && out.greeksOK;
}

// ─── Exit Check ───────────────────────────────────────────────────────────────

StrategyEngine::ExitDecision StrategyEngine::checkExit(
        const TradeEntry& trade,
        double currentLTP,
        const OptionGreeks& g,
        const TechnicalIndicators& tech) {

    ExitDecision d;

    // SL hit
    if (currentLTP <= trade.sl) {
        d.shouldExit = true;
        d.exitQty    = trade.qty;
        d.reason     = "STOP_LOSS";
        return d;
    }

    // Target 1 (partial)
    if (currentLTP >= trade.target1 && trade.qty > 0) {
        d.shouldExit = true;
        d.exitQty    = trade.qty / 2;
        d.reason     = "TARGET1_PARTIAL";
        return d;
    }

    // Target 2 (full)
    if (currentLTP >= trade.target2) {
        d.shouldExit = true;
        d.exitQty    = trade.qty;
        d.reason     = "TARGET2_FULL";
        return d;
    }

    // Greeks: delta decay too much
    if (std::abs(g.delta) < 0.15) {
        d.shouldExit = true;
        d.exitQty    = trade.qty;
        d.reason     = "DELTA_TOO_LOW";
        return d;
    }

    // IV spike exit (IV expanded > 1.5× entry IV)
    if (g.iv > 0 && trade.greeksAtEntry.iv > 0) {
        if (g.iv > trade.greeksAtEntry.iv * 1.5) {
            d.shouldExit = true;
            d.exitQty    = trade.qty;
            d.reason     = "IV_SPIKE_EXIT";
            return d;
        }
    }

    // Reversal signal: RSI crosses against our position
    if (trade.optionType == "CE") {
        // If RSI14 drops back below 45 → exit CE
        if (IndicatorsEngine::rsiCrossedBelow(tech.rsi14, tech.rsiPrev, 45.0) ||
            (!tech.emaUp && tech.rsi14 < 45)) {
            d.shouldExit = true;
            d.exitQty    = trade.qty;
            d.reason     = "RSI_REVERSAL";
            return d;
        }
    } else {
        // PE: if RSI14 crosses back above 55 → exit PE
        if (IndicatorsEngine::rsiCrossedAbove(tech.rsi14, tech.rsiPrev, 55.0) ||
            (tech.emaUp && tech.rsi14 > 55)) {
            d.shouldExit = true;
            d.exitQty    = trade.qty;
            d.reason     = "RSI_REVERSAL";
            return d;
        }
    }

    return d;  // no exit
}

// ─── Confidence Score ─────────────────────────────────────────────────────────

double StrategyEngine::scoreSignal(const StrategySignal& sig,
                                    const OptionFlowSignal& flow) const {
    double score = 0.0;
    if (sig.rsiOK)   score += 25;
    if (sig.emaOK)   score += 25;
    if (sig.vwapOK)  score += 20;
    if (sig.flowOK)  score += 20;
    if (sig.greeksOK)score += 10;
    // Bonus: strong flow alignment
    score += std::min(10.0, std::abs(OptionChainAnalyzer::getFlowScore(flow)) / 10.0);
    return std::min(100.0, score);
}

// ─── Position Sizing ──────────────────────────────────────────────────────────

double StrategyEngine::calcPositionSize(double premium, double availableCapital) const {
    // Risk 2% of capital per trade
    double riskAmount = availableCapital * 0.02;
    double slLoss     = premium * risk_.slPct;
    if (slLoss <= 0) return 1;

    int lotSize = (cfg_.tradeNifty) ? cfg_.niftyLotSize : cfg_.finniftyLotSize;
    double lotsRaw = riskAmount / (slLoss * lotSize);
    return std::max(1.0, std::floor(lotsRaw));
}
