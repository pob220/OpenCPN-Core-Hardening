"""Small RFC 6455 client used for OpenCPN's subscription-only event stream."""

from __future__ import annotations

import base64
import json
import os
import socket
import ssl
import struct
from collections.abc import Iterator, Sequence
from typing import Any
from urllib.parse import urlsplit


class _BufferedConnection:
    def __init__(self, connection: socket.socket):
        self.connection = connection
        self.buffer = bytearray()

    def take(self, count: int) -> bytes:
        while len(self.buffer) < count:
            chunk = self.connection.recv(65536)
            if not chunk:
                raise ConnectionError("OpenCPN event stream closed")
            self.buffer.extend(chunk)
        result = bytes(self.buffer[:count])
        del self.buffer[:count]
        return result

    def until(self, marker: bytes) -> bytes:
        while marker not in self.buffer:
            chunk = self.connection.recv(65536)
            if not chunk:
                raise ConnectionError("OpenCPN closed during WebSocket upgrade")
            self.buffer.extend(chunk)
        end = self.buffer.index(marker) + len(marker)
        result = bytes(self.buffer[:end])
        del self.buffer[:end]
        return result


def _send_frame(connection: socket.socket, opcode: int, payload: bytes) -> None:
    key = os.urandom(4)
    if len(payload) < 126:
        header = bytes((0x80 | opcode, 0x80 | len(payload)))
    elif len(payload) <= 0xFFFF:
        header = bytes((0x80 | opcode, 0xFE)) + struct.pack("!H", len(payload))
    else:
        header = bytes((0x80 | opcode, 0xFF)) + struct.pack("!Q", len(payload))
    masked = bytes(value ^ key[index % 4]
                   for index, value in enumerate(payload))
    connection.sendall(header + key + masked)


def _receive_frame(connection: _BufferedConnection) -> tuple[int, bytes]:
    first, length = connection.take(2)
    if first & 0x70:
        raise ConnectionError("Unsupported WebSocket extension frame")
    size = length & 0x7F
    if size == 126:
        size = struct.unpack("!H", connection.take(2))[0]
    elif size == 127:
        size = struct.unpack("!Q", connection.take(8))[0]
    key = connection.take(4) if length & 0x80 else b""
    payload = connection.take(size)
    if key:
        payload = bytes(value ^ key[index % 4]
                        for index, value in enumerate(payload))
    return first & 0x0F, payload


def iter_events(base_url: str, token: str, subscriptions: Sequence[str], *,
                verify_tls: bool = True, timeout: float = 30.0
                ) -> Iterator[dict[str, Any]]:
    """Yield the initial snapshot, subscription acknowledgement and batches."""
    target = urlsplit(base_url.rstrip("/") + "/api/v2/events")
    secure = target.scheme in {"https", "wss"}
    port = target.port or (443 if secure else 80)
    raw = socket.create_connection((target.hostname or "127.0.0.1", port),
                                   timeout=timeout)
    try:
        if secure:
            context = ssl.create_default_context()
            if not verify_tls:
                context.check_hostname = False
                context.verify_mode = ssl.CERT_NONE
            raw = context.wrap_socket(raw, server_hostname=target.hostname)
        raw.settimeout(timeout)
        buffered = _BufferedConnection(raw)
        key = base64.b64encode(os.urandom(16)).decode("ascii")
        path = target.path or "/"
        request = (
            f"GET {path} HTTP/1.1\r\nHost: {target.hostname}:{port}\r\n"
            "Upgrade: websocket\r\nConnection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n"
            f"Authorization: Bearer {token}\r\n\r\n"
        )
        raw.sendall(request.encode("ascii"))
        status = buffered.until(b"\r\n\r\n").split(b"\r\n", 1)[0]
        if b" 101 " not in status:
            raise ConnectionError(status.decode("ascii", "replace"))
        subscribed = False
        while True:
            opcode, payload = _receive_frame(buffered)
            if opcode == 8:
                return
            if opcode == 9:
                _send_frame(raw, 10, payload)
                continue
            if opcode != 1:
                continue
            event = json.loads(payload)
            yield event
            if not subscribed and event.get("type") == "snapshot":
                _send_frame(raw, 1, json.dumps(
                    {"subscribe": list(subscriptions)},
                    separators=(",", ":")).encode())
                subscribed = True
    finally:
        try:
            _send_frame(raw, 8, b"")
        except OSError:
            pass
        raw.close()
