#!/usr/bin/env python3
"""Dependency-free authenticated WebSocket probe for Test-OpenCPN."""

from __future__ import annotations

import argparse
import base64
import json
import os
import socket
import ssl
import struct
from urllib.parse import urlsplit


class BufferedConnection:
    def __init__(self, connection: socket.socket):
        self.connection = connection
        self.buffer = bytearray()

    def take(self, count: int) -> bytes:
        while len(self.buffer) < count:
            chunk = self.connection.recv(65536)
            if not chunk:
                raise RuntimeError("WebSocket closed unexpectedly")
            self.buffer.extend(chunk)
        result = bytes(self.buffer[:count])
        del self.buffer[:count]
        return result

    def until(self, marker: bytes) -> bytes:
        while marker not in self.buffer:
            chunk = self.connection.recv(65536)
            if not chunk:
                raise RuntimeError("Connection closed during HTTP upgrade")
            self.buffer.extend(chunk)
        end = self.buffer.index(marker) + len(marker)
        result = bytes(self.buffer[:end])
        del self.buffer[:end]
        return result


def receive_text(connection: BufferedConnection) -> str:
    first, length = connection.take(2)
    opcode = first & 0x0F
    masked = bool(length & 0x80)
    size = length & 0x7F
    if size == 126:
        size = struct.unpack("!H", connection.take(2))[0]
    elif size == 127:
        size = struct.unpack("!Q", connection.take(8))[0]
    key = connection.take(4) if masked else b""
    payload = connection.take(size)
    if masked:
        payload = bytes(value ^ key[index % 4]
                        for index, value in enumerate(payload))
    if opcode == 8:
        raise RuntimeError("Server closed the WebSocket")
    if opcode != 1:
        raise RuntimeError(f"Expected text frame, received opcode {opcode}")
    return payload.decode("utf-8")


def send_text(connection: socket.socket, text: str) -> None:
    payload = text.encode("utf-8")
    key = os.urandom(4)
    if len(payload) < 126:
        header = bytes((0x81, 0x80 | len(payload)))
    elif len(payload) <= 0xFFFF:
        header = bytes((0x81, 0xFE)) + struct.pack("!H", len(payload))
    else:
        header = bytes((0x81, 0xFF)) + struct.pack("!Q", len(payload))
    masked = bytes(value ^ key[index % 4]
                   for index, value in enumerate(payload))
    connection.sendall(header + key + masked)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--url", default="https://127.0.0.1:8443/api/v2/events")
    parser.add_argument("--token-file", required=True)
    parser.add_argument("--subscribe", nargs="*",
                        default=["navigation", "route-catalogue", "active-route"])
    parser.add_argument("--insecure", action="store_true")
    args = parser.parse_args()

    target = urlsplit(args.url)
    if target.scheme not in {"ws", "wss", "http", "https"}:
        raise RuntimeError("Unsupported WebSocket URL scheme")
    port = target.port or (443 if target.scheme in {"wss", "https"} else 80)
    raw = socket.create_connection((target.hostname, port), timeout=10)
    if target.scheme in {"wss", "https"}:
        context = ssl.create_default_context()
        if args.insecure:
            context.check_hostname = False
            context.verify_mode = ssl.CERT_NONE
        raw = context.wrap_socket(raw, server_hostname=target.hostname)
    raw.settimeout(10)
    connection = BufferedConnection(raw)

    token = open(args.token_file, encoding="utf-8").read().strip()
    websocket_key = base64.b64encode(os.urandom(16)).decode("ascii")
    path = target.path or "/"
    if target.query:
        path += "?" + target.query
    request = (
        f"GET {path} HTTP/1.1\r\nHost: {target.hostname}:{port}\r\n"
        "Upgrade: websocket\r\nConnection: Upgrade\r\n"
        f"Sec-WebSocket-Key: {websocket_key}\r\nSec-WebSocket-Version: 13\r\n"
        f"Authorization: Bearer {token}\r\n\r\n"
    )
    raw.sendall(request.encode("ascii"))
    response = connection.until(b"\r\n\r\n")
    status = response.split(b"\r\n", 1)[0]
    if b" 101 " not in status:
        raise RuntimeError(status.decode("ascii", "replace"))

    snapshot = json.loads(receive_text(connection))
    if snapshot.get("type") != "snapshot":
        raise RuntimeError("First event frame was not an initial snapshot")
    print(json.dumps(snapshot, sort_keys=True))
    send_text(raw, json.dumps({"subscribe": args.subscribe}, separators=(",", ":")))
    acknowledgement = json.loads(receive_text(connection))
    if acknowledgement.get("type") != "subscribed":
        raise RuntimeError("Server did not acknowledge the subscription")
    print(json.dumps(acknowledgement, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
