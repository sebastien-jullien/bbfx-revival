#!/usr/bin/env python3
"""Send a single command to BBFx Studio Debugger via TCP and print response."""
import socket, sys, time

host, port = "127.0.0.1", 33195
cmd = " ".join(sys.argv[1:]) if len(sys.argv) > 1 else "dbg.help()"

try:
    sock = socket.create_connection((host, port), timeout=5)
    sock.sendall((cmd + "\n").encode("utf-8"))
    time.sleep(0.5)
    response = b""
    sock.settimeout(1.0)
    try:
        while True:
            data = sock.recv(4096)
            if not data:
                break
            response += data
    except socket.timeout:
        pass
    sock.close()
    print(response.decode("utf-8", errors="replace").strip())
except ConnectionRefusedError:
    print("ERROR: Studio not running or TCP server not active on port 33195")
    sys.exit(1)
except Exception as e:
    print(f"ERROR: {e}")
    sys.exit(1)
