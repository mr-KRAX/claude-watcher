#!/usr/bin/env python3
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ipc_send


def main():
    sys.stdin.read()  # consume stdin
    ipc_send.send("WAITING_URGENT")


if __name__ == "__main__":
    main()
