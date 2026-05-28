# Nifty 50 & FinNifty Scalping Bot (C++)

Automated options scalping bot for **NSE Nifty 50** and **FinNifty** weekly options using the **Fyers Broker API**.

---

## Strategy Overview

### Indicators Used (Minimal by Design)
| Indicator | Purpose |
|-----------|---------|
| **RSI(14)** | Momentum filter – avoid overbought/oversold extremes |
| **RSI(7)** | Short-term momentum confirmation |
| **EMA(9)** | Fast trend direction |
| **EMA(20)** | Trend baseline |
| **VWAP** | Intraday price anchor (only trade in VWAP direction) |

### Option Chain Analysis (Core Edge)
| Metric | Signal |
|--------|--------|
| **PCR (OI-based)** | < 0.85 → bullish, > 1.2 → bearish |
| **Dominant Flow** | PUT_WRITING → bullish, CALL_WRITING → bearish |
| **Max Pain Strike** | Gravitational level for expiry |
| **IV Skew** | Put skew > 2 → bearish bias |
| **OI Build/Unwind** | Near-ATM ±5 strikes monitored |
| **Bid-Ask Imbalance** | Institutional flow direction |

### Entry Conditions

**BUY CE (Bullish Scalp):**
1. RSI(14) crosses above 40 **OR** RSI(7) > 55 (and RSI14 < 72 – not overbought)
2. EMA9 > EMA20 (uptrend)
3. Spot > VWAP
4. PCR < 0.90 **OR** dominant flow = PUT_WRITING
5. No strong call wall within 0.5% above spot
6. Best CE strike: delta 0.30–0.70, IV within 10–60%

**BUY PE (Bearish Scalp):**
1. RSI(14) crosses below 60 **OR** RSI(7) < 45 (and RSI14 > 28 – not oversold)
2. EMA9 < EMA20 (downtrend)
3. Spot < VWAP
4. PCR > 1.10 **OR** dominant flow = CALL_WRITING
5. No strong put support within 0.5% below spot
6. Best PE strike: delta 0.30–0.70, IV within 10–60%

### Exit Conditions (in priority order)
| Trigger | Action |
|---------|--------|
| Premium drops 30% from entry | Full exit (Stop Loss) |
| Premium gains 40% | Exit 50% quantity (Target 1) |
| Premium gains 80% | Exit remaining (Target 2) |
| After T1: premium pulls back 30% from peak | Trailing SL exit |
| RSI reverses against position direction | Exit (momentum exit) |
| Delta drops below 0.15 | Exit (delta decay) |
| 15:15 IST | Forced exit (time-based) |
| IV expands 1.5× entry IV | Exit (IV spike protection) |

---

## Project Structure

```
trading_bot/
├── include/
│   ├── TradingBot.h           # Core data structures
│   ├── FyersAPI.h             # Fyers REST API client
│   ├── IndicatorsEngine.h     # RSI, EMA, VWAP
│   ├── OptionChainAnalyzer.h  # Option chain flow analysis
│   ├── StrategyEngine.h       # Signal generation logic
│   └── RiskManager.h          # Position sizing & risk
├── src/
│   ├── main.cpp               # Bot orchestrator & main loop
│   ├── FyersAPI.cpp           # HTTP calls (libcurl + nlohmann/json)
│   ├── IndicatorsEngine.cpp   # Indicator math
│   ├── OptionChainAnalyzer.cpp# PCR, max pain, Greeks filters
│   ├── StrategyEngine.cpp     # Entry/exit logic
│   └── RiskManager.cpp        # Risk enforcement
├── fyers_auth.py              # One-click token generator
├── build_and_run.sh           # Build + run helper
└── CMakeLists.txt             # CMake build config
```

---

## Prerequisites

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y cmake g++ libcurl4-openssl-dev uuid-dev python3-pip

pip3 install requests
```

---

## Setup & Authentication

### Step 1 – Create Fyers API App
1. Log in at [myaccount.fyers.in](https://myaccount.fyers.in)
2. Go to **API** → **Create App**
3. Set redirect URI to `http://localhost:8080`
4. Note your **App ID** and **Secret Key**

### Step 2 – Generate Access Token (daily)
```bash
python3 fyers_auth.py \
    --app-id  "YOUR_APP_ID-100" \
    --secret  "YOUR_SECRET_KEY" \
    --redirect-uri "http://localhost:8080"
```
This opens a browser, handles login, and saves credentials to `.env`.

### Step 3 – Build & Run
```bash
chmod +x build_and_run.sh
./build_and_run.sh
```

Or manually:
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
cd ..

export FYERS_APP_ID="YOUR_APP_ID-100"
export FYERS_ACCESS_TOKEN="$(cat .env | grep TOKEN | cut -d= -f2)"
export PAPER_TRADE="1"   # 1 = paper mode, 0 = live

./build/nifty_scalp_bot
```

---

## Risk Configuration (in `main.cpp`)

```cpp
cfg.risk.maxDailyLoss    = -5000.0;   // Stop trading if daily loss > ₹5,000
cfg.risk.slPct           = 0.30;      // 30% stop loss on premium
cfg.risk.target1Pct      = 0.40;      // 40% → partial exit (50% qty)
cfg.risk.target2Pct      = 0.80;      // 80% → full exit
cfg.risk.trailSlPct      = 0.30;      // Trail SL 30% below peak after T1
cfg.risk.maxOpenTrades   = 2;         // Max 2 concurrent positions
cfg.risk.maxTradesPerDay = 10;        // Max 10 trades/day
cfg.risk.minDelta        = 0.30;      // Don't trade deep OTM
cfg.risk.maxDelta        = 0.70;      // Don't trade deep ITM
cfg.risk.minIV           = 10.0;      // Skip if IV too low
cfg.risk.maxIV           = 60.0;      // Skip if IV too high (risk of crush)
```

---

## Important Warnings

> ⚠️ **ALWAYS start with `PAPER_TRADE=1`** and run for at least 2–3 weeks before going live.

> ⚠️ **Access tokens expire daily.** Re-run `fyers_auth.py` each morning before market open.

> ⚠️ **Option scalping carries high risk.** Theta decay and bid-ask spreads erode P&L rapidly. This bot is a starting framework — backtest and optimize before live use.

> ⚠️ **Fyers API rate limits:** ~10 req/sec. The bot's 60-second loop is well within limits.

---

## Extending the Bot

- **Add more strikes monitoring:** Increase `atmRange` in `OptionChainAnalyzer::analyze()`
- **Add Telegram alerts:** Call Telegram Bot API from `executeTrade()` / `monitorTrades()`
- **WebSocket for real-time data:** Replace polling in `runBot()` with Fyers WebSocket feed
- **Backtesting:** Feed historical candles + option chain data into `StrategyEngine::evaluate()`

---

## License
Educational use only. Not financial advice. Trade at your own risk.
