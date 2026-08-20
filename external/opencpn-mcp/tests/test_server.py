import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "src"))
from opencpn_mcp.server import Server


class FakeClient:
    def status(self): return {"ready": True}
    def navigation(self): return {"stale": True}
    def list_routes(self): return []
    def active_route(self): return {"active": False}


class ServerTest(unittest.TestCase):
    def setUp(self): self.server = Server(FakeClient())
    def test_tools_exclude_activation_and_raw_messages(self):
        names = {tool["name"] for tool in self.server.tools()}
        self.assertNotIn("activate_route", names)
        self.assertNotIn("send_message", names)
        self.assertIn("get_navigation_state", names)
    def test_protocol_without_llm(self):
        result = self.server.handle({"jsonrpc": "2.0", "id": 1, "method": "tools/call",
                                     "params": {"name": "get_navigation_state", "arguments": {}}})
        self.assertIn('"stale": true', result["result"]["content"][0]["text"])
        self.assertTrue(result["result"]["structuredContent"]["result"]["stale"])
    def test_current_stateless_discovery(self):
        result = self.server.handle({"jsonrpc": "2.0", "id": 3,
                                     "method": "server/discover", "params": {}})
        self.assertIn("2026-07-28", result["result"]["protocolVersions"])
        self.assertIn("io.modelcontextprotocol/serverInfo", result["result"]["_meta"])
    def test_legacy_initialization_remains_compatible(self):
        result = self.server.handle({"jsonrpc": "2.0", "id": 4, "method": "initialize",
                                     "params": {"protocolVersion": "2025-06-18"}})
        self.assertEqual(result["result"]["protocolVersion"], "2025-06-18")
    def test_notification_has_no_response(self):
        self.assertIsNone(self.server.handle({"jsonrpc": "2.0",
                                              "method": "notifications/initialized"}))
    def test_prohibited_tool_is_rejected(self):
        result = self.server.handle({"jsonrpc": "2.0", "id": 2, "method": "tools/call",
                                     "params": {"name": "activate_route", "arguments": {}}})
        self.assertIn("error", result)


if __name__ == "__main__": unittest.main()
