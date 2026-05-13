#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

echo "==> Installing dependencies..."
python3 -m venv .venv
source .venv/bin/activate
pip install -q -r requirements.txt

echo "==> Building ClaudeWatcher.app..."
rm -rf build dist
python setup.py py2app 2>&1 | tail -10

APP_SRC="$SCRIPT_DIR/dist/ClaudeWatcher.app"
APP_DST="/Applications/ClaudeWatcher.app"

echo "==> Installing to /Applications..."
rm -rf "$APP_DST"
cp -r "$APP_SRC" "$APP_DST"

PLIST_DIR="$HOME/Library/LaunchAgents"
PLIST="$PLIST_DIR/com.kraalex.claude-watcher.plist"
mkdir -p "$PLIST_DIR"

cat > "$PLIST" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.kraalex.claude-watcher</string>
    <key>ProgramArguments</key>
    <array>
        <string>/Applications/ClaudeWatcher.app/Contents/MacOS/ClaudeWatcher</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <true/>
</dict>
</plist>
EOF

launchctl unload "$PLIST" 2>/dev/null || true
launchctl load "$PLIST"
echo "==> Done. ClaudeWatcher is running and will start on login."
