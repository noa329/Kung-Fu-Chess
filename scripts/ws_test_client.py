#!/usr/bin/env python3
"""Minimal interactive WebSocket test client for manually verifying the
kung-fu-chess server (see docs/tasks/server-phase-plan.md, task A0).

Deliberately implements the RFC 6455 handshake and text-frame
send/receive by hand with nothing but the Python standard library
(socket/hashlib/base64/struct) - no `pip install` needed, so this stays a
one-command tool on any machine with Python 3, matching every later
phase's manual-verification step.

Usage:
    python scripts/ws_test_client.py [--host 127.0.0.1] [--port 9002]

Type a line and press Enter to send it as a text frame. Anything the
server sends is printed on its own line prefixed with "< ". Ctrl+C to quit.
"""
import argparse
import base64
import hashlib
import os
import socket
import struct
import sys
import threading

WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


def handshake(sock: socket.socket, host: str, port: int, path: str) -> None:
    key = base64.b64encode(os.urandom(16)).decode("ascii")
    request = (
        f"GET {path} HTTP/1.1\r\n"
        f"Host: {host}:{port}\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        f"Sec-WebSocket-Key: {key}\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n"
    )
    sock.sendall(request.encode("ascii"))

    response = b""
    while b"\r\n\r\n" not in response:
        chunk = sock.recv(4096)
        if not chunk:
            raise ConnectionError("server closed the connection during handshake")
        response += chunk

    header_text = response.split(b"\r\n\r\n", 1)[0].decode("iso-8859-1")
    status_line = header_text.split("\r\n", 1)[0]
    if " 101 " not in status_line:
        raise ConnectionError(f"handshake rejected: {status_line}")

    expected_accept = base64.b64encode(
        hashlib.sha1((key + WS_GUID).encode("ascii")).digest()
    ).decode("ascii")
    headers = dict(
        line.split(": ", 1) for line in header_text.split("\r\n")[1:] if ": " in line
    )
    if headers.get("Sec-WebSocket-Accept") != expected_accept:
        raise ConnectionError("handshake failed: Sec-WebSocket-Accept mismatch")


def send_text_frame(sock: socket.socket, message: str) -> None:
    payload = message.encode("utf-8")
    length = len(payload)
    header = bytearray()
    header.append(0x80 | 0x1)  # FIN + text opcode

    if length <= 125:
        header.append(0x80 | length)  # masked
    elif length <= 0xFFFF:
        header.append(0x80 | 126)
        header += struct.pack(">H", length)
    else:
        header.append(0x80 | 127)
        header += struct.pack(">Q", length)

    mask = os.urandom(4)
    header += mask
    masked_payload = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
    sock.sendall(bytes(header) + masked_payload)


def recv_exact(sock: socket.socket, n: int) -> bytes:
    data = b""
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            raise ConnectionError("connection closed")
        data += chunk
    return data


def recv_frame(sock: socket.socket):
    """Returns (opcode, payload_bytes), or (None, None) on a close frame."""
    first_two = recv_exact(sock, 2)
    opcode = first_two[0] & 0x0F
    masked = first_two[1] & 0x80
    length = first_two[1] & 0x7F

    if length == 126:
        length = struct.unpack(">H", recv_exact(sock, 2))[0]
    elif length == 127:
        length = struct.unpack(">Q", recv_exact(sock, 8))[0]

    mask = recv_exact(sock, 4) if masked else None
    payload = recv_exact(sock, length) if length else b""
    if masked:
        payload = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))

    if opcode == 0x8:  # close
        return None, None
    return opcode, payload


def listen_loop(sock: socket.socket) -> None:
    try:
        while True:
            opcode, payload = recv_frame(sock)
            if opcode is None:
                print("\n[server closed the connection]")
                os._exit(0)
            if opcode == 0x1:  # text
                print(f"\n< {payload.decode('utf-8', errors='replace')}\n> ", end="", flush=True)
    except (ConnectionError, OSError) as e:
        print(f"\n[connection lost: {e}]")
        os._exit(1)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9002)
    parser.add_argument("--path", default="/")
    args = parser.parse_args()

    sock = socket.create_connection((args.host, args.port), timeout=10)
    sock.settimeout(None)
    handshake(sock, args.host, args.port, args.path)
    print(f"Connected to ws://{args.host}:{args.port}{args.path}")

    threading.Thread(target=listen_loop, args=(sock,), daemon=True).start()

    try:
        while True:
            line = input("> ")
            if line:
                send_text_frame(sock, line)
    except (EOFError, KeyboardInterrupt):
        print()
    finally:
        sock.close()


if __name__ == "__main__":
    sys.exit(main())
