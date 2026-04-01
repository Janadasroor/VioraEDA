#!/usr/bin/env python3
"""
VioSpice Share Server
Lightweight backend for sharing schematics via short URLs.

Usage:
    python share_server.py [--port 8765] [--persist db_path]

Endpoints:
    POST /api/share     - Upload schematic, returns shortcode
    GET  /api/share/<id> - Download schematic by shortcode
    DELETE /api/share/<id> - Delete schematic
"""

import argparse
import json
import os
import sys
import time
import uuid
from datetime import datetime, timedelta
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
import threading

try:
    import nanoid
except ImportError:
    nanoid = None

STORAGE = {}
STORAGE_LOCK = threading.Lock()
EXPIRY_DAYS = 30


def generate_shortcode():
    """Generate a 6-character shortcode."""
    if nanoid:
        return nanoid.generate(size=6)
    return uuid.uuid4().hex[:6]


def cleanup_expired():
    """Remove expired entries periodically."""
    while True:
        time.sleep(3600)
        with STORAGE_LOCK:
            now = datetime.now()
            expired = []
            for code, data in STORAGE.items():
                if "expiresAt" in data:
                    expires = datetime.fromisoformat(data["expiresAt"])
                    if now > expires:
                        expired.append(code)
            for code in expired:
                del STORAGE[code]
            if expired:
                print(f"Cleaned up {len(expired)} expired shares")


class ShareRequestHandler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        print(f"[{self.address_string()}] {format % args}")

    def send_json_response(self, status, data):
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(json.dumps(data).encode())

    def do_GET(self):
        parsed = urlparse(self.path)

        if parsed.path == "/api/share":
            with STORAGE_LOCK:
                codes = list(STORAGE.keys())
            self.send_json_response(200, {"count": len(codes), "shares": codes})
            return

        if parsed.path.startswith("/api/share/"):
            code = parsed.path.split("/")[-1]
            with STORAGE_LOCK:
                data = STORAGE.get(code)

            if not data:
                self.send_json_response(404, {"error": "Not found"})
                return

            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(data["content"].encode())
            return

        self.send_json_response(404, {"error": "Not found"})

    def do_POST(self):
        parsed = urlparse(self.path)

        if parsed.path != "/api/share":
            self.send_json_response(404, {"error": "Not found"})
            return

        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length)

        try:
            data = json.loads(body)
        except json.JSONDecodeError:
            self.send_json_response(400, {"error": "Invalid JSON"})
            return

        schematic = data.get("schematic", "")
        title = data.get("title", "")
        description = data.get("description", "")

        if not schematic:
            self.send_json_response(400, {"error": "No schematic data"})
            return

        code = generate_shortcode()
        expiresAt = (datetime.now() + timedelta(days=EXPIRY_DAYS)).isoformat()

        share_data = {
            "schematic": schematic,
            "title": title,
            "description": description,
            "createdAt": datetime.now().isoformat(),
            "expiresAt": expiresAt,
        }

        with STORAGE_LOCK:
            STORAGE[code] = share_data

        short_url = f"viospice://share/{code}"
        full_url = f"https://viospice.com/share/{code}"

        self.send_json_response(
            201,
            {
                "shortcode": code,
                "shortUrl": short_url,
                "fullUrl": full_url,
                "expiresAt": expiresAt,
            },
        )

    def do_DELETE(self):
        parsed = urlparse(self.path)

        if not parsed.path.startswith("/api/share/"):
            self.send_json_response(404, {"error": "Not found"})
            return

        code = parsed.path.split("/")[-1]

        with STORAGE_LOCK:
            if code in STORAGE:
                del STORAGE[code]
                self.send_json_response(200, {"deleted": True})
            else:
                self.send_json_response(404, {"error": "Not found"})


def main():
    parser = argparse.ArgumentParser(description="VioSpice Share Server")
    parser.add_argument("--port", type=int, default=8765, help="Port to listen on")
    parser.add_argument(
        "--persist", type=str, help="Path to persist storage (JSON file)"
    )
    args = parser.parse_args()

    if args.persist and os.path.exists(args.persist):
        try:
            with open(args.persist, "r") as f:
                loaded = json.load(f)
                with STORAGE_LOCK:
                    STORAGE = loaded
                print(f"Loaded {len(STORAGE)} shares from {args.persist}")
        except Exception as e:
            print(f"Warning: Could not load storage: {e}")

    server = HTTPServer(("0.0.0.0", args.port), ShareRequestHandler)
    print(f"VioSpice Share Server running on http://0.0.0.0:{args.port}")
    print(f"Endpoints:")
    print(f"  POST   /api/share         - Upload schematic")
    print(f"  GET    /api/share/<code>  - Download schematic")
    print(f"  DELETE /api/share/<code>  - Delete schematic")

    cleanup_thread = threading.Thread(target=cleanup_expired, daemon=True)
    cleanup_thread.start()

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down...")

        if args.persist:
            try:
                with open(args.persist, "w") as f:
                    with STORAGE_LOCK:
                        json.dump(STORAGE, f)
                print(f"Saved {len(STORAGE)} shares to {args.persist}")
            except Exception as e:
                print(f"Warning: Could not save storage: {e}")

    server.server_close()


if __name__ == "__main__":
    main()
