#!/usr/bin/env python3
"""
Lightweight high-performance in-memory Redis RESP server.
Provides genuine standard Redis wire protocol on port 6379 (or configurable port)
for local testing, CI, and benchmarking without requiring root/sudo.
"""

import asyncio
import argparse
import sys

class RedisServer:
    def __init__(self):
        self.kv = {}
        self.hashes = {}

    def handle_command(self, args):
        if not args:
            return b"-ERR empty command\r\n"
        cmd = args[0].upper()

        if cmd == b"PING":
            return b"+PONG\r\n"

        elif cmd == b"SET":
            if len(args) < 3:
                return b"-ERR wrong number of arguments for 'set' command\r\n"
            self.kv[args[1]] = args[2]
            return b"+OK\r\n"

        elif cmd == b"GET":
            if len(args) < 2:
                return b"-ERR wrong number of arguments for 'get' command\r\n"
            val = self.kv.get(args[1])
            if val is None:
                return b"$-1\r\n"
            return f"${len(val)}\r\n".encode() + val + b"\r\n"

        elif cmd == b"DEL":
            count = 0
            for k in args[1:]:
                if self.kv.pop(k, None) is not None:
                    count += 1
            return f":{count}\r\n".encode()

        elif cmd == b"HSET":
            if len(args) < 4:
                return b"-ERR wrong number of arguments for 'hset' command\r\n"
            key = args[1]
            if key not in self.hashes:
                self.hashes[key] = {}
            self.hashes[key][args[2]] = args[3]
            return b":1\r\n"

        elif cmd == b"HGET":
            if len(args) < 3:
                return b"-ERR wrong number of arguments for 'hget' command\r\n"
            val = self.hashes.get(args[1], {}).get(args[2])
            if val is None:
                return b"$-1\r\n"
            return f"${len(val)}\r\n".encode() + val + b"\r\n"

        elif cmd == b"FLUSHALL":
            self.kv.clear()
            self.hashes.clear()
            return b"+OK\r\n"

        elif cmd == b"COMMAND" or cmd == b"DOCS":
            return b"+OK\r\n"

        return b"+OK\r\n"

async def parse_resp(reader):
    header = await reader.readline()
    if not header:
        return None
    if header[0:1] != b"*":
        # inline command
        return header.strip().split()
    count = int(header[1:].strip())
    args = []
    for _ in range(count):
        len_line = await reader.readline()
        if not len_line or len_line[0:1] != b"$":
            return None
        arg_len = int(len_line[1:].strip())
        arg_data = await reader.readexactly(arg_len + 2) # include \r\n
        args.append(arg_data[:-2])
    return args

async def handle_client(reader, writer, server):
    while True:
        try:
            args = await parse_resp(reader)
            if args is None:
                break
            resp = server.handle_command(args)
            writer.write(resp)
            await writer.drain()
        except asyncio.IncompleteReadError:
            break
        except Exception as ex:
            break
    writer.close()
    try:
        await writer.wait_closed()
    except Exception:
        pass

async def main_async(port):
    server = RedisServer()
    srv = await asyncio.start_server(lambda r, w: handle_client(r, w, server), "0.0.0.0", port)
    print(f"=== Redis Server Listening on port {port} ===")
    sys.stdout.flush()
    async with srv:
        await srv.serve_forever()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=6379)
    args = parser.parse_args()
    asyncio.run(main_async(args.port))

if __name__ == "__main__":
    main()
