#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# build_and_run.sh  –  Build the Nifty/FinNifty Scalping Bot and run it
# ─────────────────────────────────────────────────────────────────────────────
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

echo "═══════════════════════════════════════════════════"
echo "  Nifty/FinNifty Scalp Bot — Build Script"
echo "═══════════════════════════════════════════════════"

# ── Check dependencies ────────────────────────────────────────────────────────
check_dep() {
    if ! command -v "$1" &>/dev/null; then
        echo "❌  Missing: $1  →  Install with: $2"
        exit 1
    fi
}
check_dep cmake  "sudo apt install cmake"
check_dep g++    "sudo apt install g++"
check_dep curl   "sudo apt install libcurl4-openssl-dev"
echo "✅  Dependencies OK"

# ── Build ─────────────────────────────────────────────────────────────────────
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17
make -j$(nproc)
echo ""
echo "✅  Build successful: $BUILD_DIR/nifty_scalp_bot"
echo ""

# ── Load .env if present ──────────────────────────────────────────────────────
if [ -f "$SCRIPT_DIR/.env" ]; then
    echo "📄  Loading .env..."
    set -a
    source "$SCRIPT_DIR/.env"
    set +a
fi

# ── Validate credentials ──────────────────────────────────────────────────────
if [ -z "$FYERS_APP_ID" ] || [ -z "$FYERS_ACCESS_TOKEN" ]; then
    echo ""
    echo "⚠️   FYERS credentials not set!"
    echo "    First, generate your access token:"
    echo ""
    echo "    python3 fyers_auth.py --app-id <APP_ID> --secret <SECRET>"
    echo ""
    echo "    Then source the .env file or export manually."
    echo ""
    echo "    Running in PAPER TRADE mode with dummy credentials for testing..."
    export FYERS_APP_ID="TEST_APP"
    export FYERS_ACCESS_TOKEN="TEST_TOKEN"
    export PAPER_TRADE="1"
fi

echo "─────────────────────────────────────────────────────"
echo "  App ID:      $FYERS_APP_ID"
echo "  Paper Trade: ${PAPER_TRADE:-1}"
echo "─────────────────────────────────────────────────────"
echo ""

# ── Run ───────────────────────────────────────────────────────────────────────
cd "$SCRIPT_DIR"
exec "$BUILD_DIR/nifty_scalp_bot" "$@"
