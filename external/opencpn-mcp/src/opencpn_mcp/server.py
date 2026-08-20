"""Deterministic dual-era JSON-RPC MCP stdio server.

The server accepts both the stateless 2026-07-28 protocol and the preceding
2025-06-18 initialization flow.  Neither protocol path grants navigation
authority: authorization remains entirely in the OpenCPN bearer token.
"""

from __future__ import annotations

import json
import os
import sys
from typing import Any

from opencpn_control import Client, OpenCPNError


TOOLS = [
    ("get_opencpn_status", "Inspect readiness and capabilities", {}),
    ("get_navigation_state", "Read navigation state including freshness", {}),
    ("list_routes", "List routes", {}),
    ("get_route", "Get complete route geometry", {"guid": "string"}),
    ("get_active_route", "Inspect the active route without changing it", {}),
    ("validate_route", "Validate a stored route against charts",
     {"guid": "string", "minimum_depth_m": "number", "land_margin_nm": "number"}),
    ("start_route_plan", "Start a bounded chart/weather planning job",
     {"request": "object"}),
    ("get_route_plan", "Inspect a planning job and its completed result",
     {"job_id": "string"}),
    ("cancel_route_plan", "Request cancellation of a planning job",
     {"job_id": "string"}),
    ("compare_route_plans", "Return completed plan results for comparison",
     {"job_ids": "array"}),
    ("create_draft_route", "Create a draft; requires separately scoped credentials",
     {"name": "string", "waypoints": "array"}),
]

MODERN_PROTOCOL = "2026-07-28"
LEGACY_PROTOCOL = "2025-06-18"
SERVER_INFO = {"name": "opencpn-mcp", "version": "0.1.0"}
SERVER_INFO_KEY = "io.modelcontextprotocol/serverInfo"


class Server:
    def __init__(self, client: Client): self.client = client

    def tools(self) -> list[dict[str, Any]]:
        output = []
        for name, description, fields in TOOLS:
            properties = {key: {"type": kind} for key, kind in fields.items()}
            required = [key for key in fields if key not in {"land_margin_nm"}]
            output.append({"name": name, "description": description,
                           "inputSchema": {"type": "object", "properties": properties,
                                           "required": required, "additionalProperties": False}})
        return output

    def call(self, name: str, arguments: dict[str, Any]) -> Any:
        if name == "get_opencpn_status": return self.client.status()
        if name == "get_navigation_state": return self.client.navigation()
        if name == "list_routes": return self.client.list_routes()
        if name == "get_route": return self.client.get_route(arguments["guid"])
        if name == "get_active_route": return self.client.active_route()
        if name == "validate_route":
            route = self.client.get_route(arguments["guid"])
            return self.client.validate_route(
                [point["position"] for point in route["waypoints"]],
                minimum_depth_m=float(arguments["minimum_depth_m"]),
                land_margin_nm=float(arguments.get("land_margin_nm", 0)))
        if name == "start_route_plan":
            return self.client.start_plan(arguments["request"])
        if name == "get_route_plan":
            job = self.client.get_plan(arguments["job_id"])
            return ({"job": job, "result": self.client.get_plan_result(arguments["job_id"])}
                    if job["state"] == "completed" else {"job": job})
        if name == "cancel_route_plan":
            return self.client.cancel_plan(arguments["job_id"])
        if name == "compare_route_plans":
            return {job_id: self.client.get_plan_result(job_id)
                    for job_id in arguments["job_ids"]}
        if name == "create_draft_route":
            return self.client.create_draft(arguments["name"], arguments["waypoints"])
        raise ValueError(f"Unknown or prohibited tool: {name}")

    def handle(self, request: dict[str, Any]) -> dict[str, Any] | None:
        if request.get("jsonrpc") != "2.0":
            return self._error(request.get("id"), -32600, "Invalid JSON-RPC request")
        if "id" not in request:
            # Notifications never receive a JSON-RPC response.  The legacy
            # initialized notification needs no server-side state.
            return None
        response = {"jsonrpc": "2.0", "id": request["id"]}
        try:
            method = request.get("method")
            if method == "server/discover":
                response["result"] = {
                    "protocolVersions": [MODERN_PROTOCOL, LEGACY_PROTOCOL],
                    "capabilities": {"tools": {}},
                    "ttlMs": 3600000,
                    "cacheScope": "public",
                }
            elif method == "initialize":
                response["result"] = {"protocolVersion": LEGACY_PROTOCOL,
                                      "capabilities": {"tools": {}},
                                      "serverInfo": SERVER_INFO}
            elif method == "tools/list":
                response["result"] = {"tools": self.tools(), "ttlMs": 300000,
                                      "cacheScope": "private"}
            elif method == "tools/call":
                params = request.get("params", {})
                value = self.call(params.get("name", ""), params.get("arguments", {}))
                response["result"] = {
                    "content": [{"type": "text", "text": json.dumps(value)}],
                    "structuredContent": {"result": value},
                    "isError": False,
                }
            else:
                return self._error(request["id"], -32601, "Method not found")
        except (ValueError, KeyError, TypeError) as error:
            return self._error(request["id"], -32602, str(error))
        except OpenCPNError as error:
            response["result"] = {
                "content": [{"type": "text", "text": str(error)}],
                "isError": True,
            }
        response["result"].setdefault("_meta", {})[SERVER_INFO_KEY] = SERVER_INFO
        return response

    @staticmethod
    def _error(request_id: Any, code: int, message: str) -> dict[str, Any]:
        return {"jsonrpc": "2.0", "id": request_id,
                "error": {"code": code, "message": message}}


def main() -> int:
    token = os.getenv("OPENCPN_TOKEN")
    if not token:
        print("OPENCPN_TOKEN is required", file=sys.stderr); return 2
    client = Client(os.getenv("OPENCPN_URL", "https://127.0.0.1:8443"), token,
                    verify_tls=os.getenv("OPENCPN_INSECURE") != "1")
    server = Server(client)
    for line in sys.stdin:
        try: response = server.handle(json.loads(line))
        except json.JSONDecodeError as error:
            response = {"jsonrpc": "2.0", "id": None,
                        "error": {"code": -32700, "message": str(error)}}
        if response is not None:
            print(json.dumps(response, separators=(",", ":")), flush=True)
    return 0


if __name__ == "__main__": raise SystemExit(main())
