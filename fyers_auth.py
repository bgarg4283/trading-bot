#!/usr/bin/env python3
"""
Fyers Access Token Generator
Run this once before starting the C++ bot to get today's access token.

Usage:
    python3 fyers_auth.py --app-id YOUR_APP_ID --secret YOUR_SECRET --redirect-uri http://localhost:8080

Then export the token:
    export FYERS_APP_ID="YOUR_APP_ID"
    export FYERS_ACCESS_TOKEN="<token from this script>"
    export PAPER_TRADE="1"   # 1 = paper, 0 = live
"""

import argparse
import webbrowser
import http.server
import urllib.parse
import json
import hashlib
import requests

AUTH_BASE = "https://api-t1.fyers.in/api/v3"

class TokenServer(http.server.BaseHTTPRequestHandler):
    auth_code = None

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        params = urllib.parse.parse_qs(parsed.query)
        if "auth_code" in params:
            TokenServer.auth_code = params["auth_code"][0]
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b"<h2>Auth code received! You can close this tab.</h2>")
        else:
            self.send_response(400)
            self.end_headers()

    def log_message(self, *args):
        pass

def get_access_token(app_id: str, secret: str, redirect_uri: str, auth_code: str) -> str:
    app_id_hash = hashlib.sha256(f"{app_id}:{secret}".encode()).hexdigest()
    payload = {
        "grant_type": "authorization_code",
        "appIdHash": app_id_hash,
        "code": auth_code
    }
    headers = {"Content-Type": "application/json"}
    r = requests.post(f"{AUTH_BASE}/validate-authcode", json=payload, headers=headers)
    r.raise_for_status()
    data = r.json()
    if data.get("s") == "ok":
        return data["access_token"]
    raise ValueError(f"Token exchange failed: {data}")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--app-id",      required=True)
    parser.add_argument("--secret",      required=True)
    parser.add_argument("--redirect-uri",default="http://localhost:8080")
    args = parser.parse_args()

    # Step 1: Open login URL
    redirect_enc = urllib.parse.quote(args.redirect_uri, safe="")
    login_url = (f"https://api-t1.fyers.in/api/v3/generate-authcode"
                 f"?client_id={args.app_id}"
                 f"&redirect_uri={redirect_enc}"
                 f"&response_type=code&state=scalpbot")
    print(f"\nOpening browser for Fyers login...")
    webbrowser.open(login_url)

    # Step 2: Start local server to catch redirect
    port = int(args.redirect_uri.split(":")[-1])
    server = http.server.HTTPServer(("localhost", port), TokenServer)
    print(f"Waiting for auth code on port {port}...")
    while TokenServer.auth_code is None:
        server.handle_request()

    # Step 3: Exchange for access token
    token = get_access_token(args.app_id, args.secret, args.redirect_uri, TokenServer.auth_code)
    print(f"\n✅ Access Token obtained!\n")
    print(f"Run these export commands before starting the bot:")
    print(f"\n  export FYERS_APP_ID=\"{args.app_id}\"")
    print(f"  export FYERS_ACCESS_TOKEN=\"{token}\"")
    print(f"  export PAPER_TRADE=\"1\"   # Change to 0 for live trading")
    print(f"\n  ./build/nifty_scalp_bot\n")

    # Save to .env file for convenience
    with open(".env", "w") as f:
        f.write(f"FYERS_APP_ID={args.app_id}\n")
        f.write(f"FYERS_ACCESS_TOKEN={token}\n")
        f.write(f"PAPER_TRADE=1\n")
    print("  (Saved to .env for reference — delete after use!)")

if __name__ == "__main__":
    main()
