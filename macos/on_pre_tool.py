#!/usr/bin/env python3
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import serial_send


def main():
    payload = json.loads(sys.stdin.read())
    tool_name = payload.get("tool_name", "Unknown")
    serial_send.send(f"WORKING:{tool_name}")


if __name__ == "__main__":
    main()
