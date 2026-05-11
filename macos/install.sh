#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SETTINGS_FILE="$HOME/.claude/settings.json"
VENV="$SCRIPT_DIR/.venv"

# Create venv and install pyserial if not already present
if [ ! -x "$VENV/bin/python" ]; then
  echo "Creating virtualenv at $VENV..."
  python3 -m venv "$VENV"
fi
if ! "$VENV/bin/python" -c "import serial" 2>/dev/null; then
  echo "Installing pyserial..."
  "$VENV/bin/pip" install --quiet pyserial
fi

PYTHON="$VENV/bin/python"

if [ ! -f "$SETTINGS_FILE" ]; then
  mkdir -p "$(dirname "$SETTINGS_FILE")"
  echo '{}' > "$SETTINGS_FILE"
fi

"$PYTHON" - "$SETTINGS_FILE" "$SCRIPT_DIR" "$PYTHON" <<'PYEOF'
import sys, json

settings_path = sys.argv[1]
scripts_dir = sys.argv[2]
python_bin = sys.argv[3]

with open(settings_path) as f:
    settings = json.load(f)

hooks = settings.setdefault("hooks", {})

def make_entry(script):
    return {
        "matcher": ".*",
        "hooks": [{"type": "command", "command": f"{python_bin} {scripts_dir}/{script}"}]
    }

def upsert(hook_list, script):
    """Add entry if missing; update command if present but using wrong python."""
    expected_cmd = f"{python_bin} {scripts_dir}/{script}"
    for entry in hook_list:
        cmd = next(iter(entry.get("hooks") or []), {}).get("command", "")
        if cmd.endswith(script):
            if cmd != expected_cmd:
                entry["hooks"][0]["command"] = expected_cmd
            return
    hook_list.append(make_entry(script))

upsert(hooks.setdefault("PreToolUse", []), "on_pre_tool.py")
upsert(hooks.setdefault("Stop", []),        "on_stop.py")
upsert(hooks.setdefault("Notification", []), "on_notification.py")

with open(settings_path, "w") as f:
    json.dump(settings, f, indent=2)
    f.write("\n")

print(f"Hooks installed to {settings_path}")
PYEOF
