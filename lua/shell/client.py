#!/usr/bin/env python3
"""BBFx Shell Client — TCP REPL
Connects to the BBFx TCP shell server (default 127.0.0.1:33195).
Usage: python client.py [host] [port]
"""

import socket
import sys
import threading

DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 33195

def receive_loop(sock):
    """Background thread: print responses from server."""
    try:
        buf = b""
        while True:
            data = sock.recv(4096)
            if not data:
                print("\n[disconnected]")
                break
            buf += data
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                print(line.decode("utf-8", errors="replace"))
    except (ConnectionResetError, OSError):
        print("\n[connection lost]")

def main():
    host = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_HOST
    port = int(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_PORT

    print(f"BBFx Shell Client — connecting to {host}:{port}")
    try:
        sock = socket.create_connection((host, port), timeout=5)
    except (ConnectionRefusedError, OSError) as e:
        print(f"Connection failed: {e}")
        return 1

    print("Connected. Type Lua expressions. Ctrl+C to quit.\n")

    # Start background receive thread
    t = threading.Thread(target=receive_loop, args=(sock,), daemon=True)
    t.start()

    try:
        while True:
            line = input("> ")
            if line.strip().lower() == "quit":
                break
            sock.sendall((line + "\n").encode("utf-8"))
    except (KeyboardInterrupt, EOFError):
        pass
    finally:
        sock.close()
        print("Disconnected.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
