-- shell/client.lua — BBFx v2.6 TCP Shell Client
-- NOTE: The BBFx Lua runtime does not include LuaSocket.
-- Use the Python client instead:
--   python lua/shell/client.py [host] [port]
--   Default: 127.0.0.1:33195
--
-- Or use any TCP client (netcat, telnet):
--   echo "return 1+1" | nc 127.0.0.1 33195

Config = Config or {}
Config.shell = Config.shell or {host = "127.0.0.1", port = 33195}

print("[ShellClient] Use the Python client to connect:")
print("  python lua/shell/client.py " .. Config.shell.host .. " " .. Config.shell.port)
print("Or: echo 'return 1+1' | nc " .. Config.shell.host .. " " .. Config.shell.port)
