import json
import os
import sys
import threading
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "src"))
from opencpn_control import Client, OpenCPNError


class Handler(BaseHTTPRequestHandler):
    calls = 0
    def do_GET(self):
        Handler.calls += 1
        if self.headers.get("Authorization") != "Bearer secret":
            self.send_response(401); body = {"error": {"code": "invalid_token", "message": "bad"}}
        elif self.path == "/api/v2/version": self.send_response(200); body = {"apiVersion": "2.0.0"}
        elif self.path == "/api/v2/readiness": self.send_response(200); body = {"ready": True}
        elif self.path == "/api/v2/capabilities": self.send_response(200); body = {"capabilities": ["routes.query.v1"]}
        elif self.path == "/api/v2/routes": self.send_response(200); body = {"routes": []}
        else: self.send_response(404); body = {"error": {"code": "not_found", "message": "missing"}}
        data = json.dumps(body).encode(); self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data))); self.end_headers(); self.wfile.write(data)
    def log_message(self, *_): pass


class ClientTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True); cls.thread.start()
        cls.url = f"http://127.0.0.1:{cls.server.server_port}"
    @classmethod
    def tearDownClass(cls): cls.server.shutdown(); cls.server.server_close()
    def test_status_and_routes(self):
        client = Client(self.url, "secret")
        self.assertTrue(client.status()["readiness"]["ready"])
        self.assertEqual(client.list_routes(), [])
    def test_structured_error_redacts_token(self):
        with self.assertRaises(OpenCPNError) as caught: Client(self.url, "wrong").version()
        self.assertEqual(caught.exception.code, "invalid_token")
        self.assertNotIn("wrong", str(caught.exception))


if __name__ == "__main__": unittest.main()
