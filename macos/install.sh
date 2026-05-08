#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SETTINGS_FILE="$HOME/.claude/settings.json"

if [ ! -f "$SETTINGS_FILE" ]; then
  mkdir -p "$(dirname "$SETTINGS_FILE")"
  echo '{}' > "$SETTINGS_FILE"
fi

python3 - "$SETTINGS_FILE" "$SCRIPT_DIR" <<'PYEOF'
import sys, json

settings_path = sys.argv[1]
scripts_dir = sys.argv[2]

with open(settings_path) as f:
    settings = json.load(f)

hooks = settings.setdefault("hooks", {})

def make_entry(script):
    return {
        "matcher": ".*",
        "hooks": [{"type": "command", "command": f"python3 {scripts_dir}/{script}"}]
    }

# PreToolUse — match all tools
pre = hooks.setdefault("PreToolUse", [])
if not any(e.get("hooks", [{}])[0].get("command", "").endswith("on_pre_tool.py") for e in pre):
    pre.append(make_entry("on_pre_tool.py"))

# Stop
stop = hooks.setdefault("Stop", [])
if not any(e.get("hooks", [{}])[0].get("command", "").endswith("on_stop.py") for e in stop):
    stop.append(make_entry("on_stop.py"))

# Notification
notif = hooks.setdefault("Notification", [])
if not any(e.get("hooks", [{}])[0].get("command", "").endswith("on_notification.py") for e in notif):
    notif.append(make_entry("on_notification.py"))

with open(settings_path, "w") as f:
    json.dump(settings, f, indent=2)
    f.write("\n")

print(f"Hooks installed to {settings_path}")
PYEOF
