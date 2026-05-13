#!/usr/bin/env python3
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ipc_send


def main():
    payload = json.loads(sys.stdin.read())
    ipc_send.send("Stop", session_id=payload.get("session_id", ""))


if __name__ == "__main__":
    main()
