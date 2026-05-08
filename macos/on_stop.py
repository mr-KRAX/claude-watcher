#!/usr/bin/env python3
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import serial_send


def main():
    sys.stdin.read()  # consume stdin
    serial_send.send("WAITING")


if __name__ == "__main__":
    main()
