"""Standard-library OpenCPN API v2 client."""

from __future__ import annotations

import json
import ssl
import uuid
import urllib.error
import urllib.request
from dataclasses import dataclass
from typing import Any

from .events import iter_events


@dataclass
class OpenCPNError(RuntimeError):
    status: int
    code: str
    message: str

    def __str__(self) -> str:
        return f"OpenCPN API error {self.status} ({self.code}): {self.message}"


class Client:
    def __init__(self, base_url: str, token: str, *, verify_tls: bool = True,
                 timeout: float = 15.0):
        self.base_url = base_url.rstrip("/")
        self.token = token
        self.timeout = timeout
        self.verify_tls = verify_tls
        self._context = (ssl.create_default_context() if verify_tls else
                         ssl._create_unverified_context())

    def __enter__(self) -> "Client":
        return self

    def __exit__(self, *_: object) -> None:
        return None

    def _request(self, method: str, path: str, body: Any = None,
                 *, idempotency_key: str | None = None) -> dict[str, Any]:
        payload = None if body is None else json.dumps(body).encode("utf-8")
        headers = {"Authorization": f"Bearer {self.token}",
                   "Accept": "application/json"}
        if payload is not None:
            headers["Content-Type"] = "application/json"
        if idempotency_key:
            headers["Idempotency-Key"] = idempotency_key
        request = urllib.request.Request(self.base_url + path, payload, headers,
                                         method=method)
        try:
            with urllib.request.urlopen(request, timeout=self.timeout,
                                        context=self._context) as response:
                return json.loads(response.read())
        except urllib.error.HTTPError as error:
            try:
                detail = json.loads(error.read()).get("error", {})
            except (json.JSONDecodeError, UnicodeDecodeError):
                detail = {}
            raise OpenCPNError(error.code, detail.get("code", "http_error"),
                               detail.get("message", error.reason)) from None
        except urllib.error.URLError as error:
            raise OpenCPNError(0, "connection_failed", str(error.reason)) from None

    def version(self) -> dict[str, Any]:
        return self._request("GET", "/api/v2/version")

    def capabilities(self) -> dict[str, Any]:
        return self._request("GET", "/api/v2/capabilities")

    def providers(self) -> list[dict[str, Any]]:
        return self._request("GET", "/api/v2/providers")["providers"]

    def readiness(self) -> dict[str, Any]:
        return self._request("GET", "/api/v2/readiness")

    def status(self) -> dict[str, Any]:
        return {"version": self.version(), "readiness": self.readiness(),
                "capabilities": self.capabilities()["capabilities"]}

    def navigation(self) -> dict[str, Any]:
        return self._request("GET", "/api/v2/navigation")

    def list_routes(self) -> list[dict[str, Any]]:
        return self._request("GET", "/api/v2/routes")["routes"]

    def get_route(self, guid: str) -> dict[str, Any]:
        return self._request("GET", f"/api/v2/routes/{guid}")

    def active_route(self) -> dict[str, Any]:
        return self._request("GET", "/api/v2/active-route")

    def validate_route(self, route: list[dict[str, float]], *,
                       minimum_depth_m: float,
                       land_margin_nm: float = 0.0) -> dict[str, Any]:
        return self._request("POST", "/api/v2/chart-safety/validate-route",
                             {"route": route,
                              "minimumDepthMeters": minimum_depth_m,
                              "landMarginNauticalMiles": land_margin_nm})

    def create_draft(self, name: str, waypoints: list[dict[str, Any]], *,
                     idempotency_key: str | None = None) -> dict[str, Any]:
        return self._request("POST", "/api/v2/routes",
                             {"name": name, "waypoints": waypoints},
                             idempotency_key=idempotency_key or str(uuid.uuid4()))

    def update_draft(self, guid: str, expected_revision: int, name: str,
                     waypoints: list[dict[str, Any]], *,
                     idempotency_key: str | None = None) -> dict[str, Any]:
        return self._request("PUT", f"/api/v2/routes/{guid}",
                             {"expectedRevision": expected_revision,
                              "name": name, "waypoints": waypoints},
                             idempotency_key=idempotency_key or str(uuid.uuid4()))

    def delete_route(self, guid: str, expected_revision: int, *,
                     idempotency_key: str | None = None) -> dict[str, Any]:
        return self._request("DELETE", f"/api/v2/routes/{guid}",
                             {"expectedRevision": expected_revision},
                             idempotency_key=idempotency_key or str(uuid.uuid4()))

    def activate_route(self, guid: str, *, waypoint_guid: str | None = None,
                       idempotency_key: str | None = None) -> dict[str, Any]:
        body = {} if waypoint_guid is None else {"waypointGuid": waypoint_guid}
        return self._request("POST", f"/api/v2/routes/{guid}/activate", body,
                             idempotency_key=idempotency_key or str(uuid.uuid4()))

    def deactivate_route(self, *, idempotency_key: str | None = None) -> dict[str, Any]:
        return self._request("POST", "/api/v2/routes/deactivate",
                             idempotency_key=idempotency_key or str(uuid.uuid4()))

    def start_plan(self, request: dict[str, Any], *,
                   idempotency_key: str | None = None) -> dict[str, Any]:
        return self._request("POST", "/api/v2/planning/jobs", request,
                             idempotency_key=idempotency_key or str(uuid.uuid4()))

    def get_plan(self, job_id: str) -> dict[str, Any]:
        return self._request("GET", f"/api/v2/planning/jobs/{job_id}")

    def cancel_plan(self, job_id: str) -> dict[str, Any]:
        return self._request("DELETE", f"/api/v2/planning/jobs/{job_id}")

    def get_plan_result(self, job_id: str) -> dict[str, Any]:
        return self._request("GET", f"/api/v2/planning/jobs/{job_id}/result")

    def start_environment(self, request: dict[str, Any], *,
                          idempotency_key: str | None = None) -> dict[str, Any]:
        return self._request("POST", "/api/v2/environment/jobs", request,
                             idempotency_key=idempotency_key or str(uuid.uuid4()))

    def get_environment_job(self, job_id: str) -> dict[str, Any]:
        return self._request("GET", f"/api/v2/environment/jobs/{job_id}")

    def cancel_environment_job(self, job_id: str) -> dict[str, Any]:
        return self._request("DELETE", f"/api/v2/environment/jobs/{job_id}")

    def get_environment_result(self, job_id: str) -> dict[str, Any]:
        return self._request("GET", f"/api/v2/environment/jobs/{job_id}/result")

    def list_environment_datasets(self) -> list[dict[str, Any]]:
        return self._request("GET", "/api/v2/environment/datasets")["datasets"]

    def activate_environment_dataset(self, identity: str) -> dict[str, Any]:
        return self._request(
            "POST", f"/api/v2/environment/datasets/{identity}/activate")

    def events(self, subscriptions: list[str] | None = None):
        """Iterate the initial snapshot, acknowledgement and event batches."""
        return iter_events(
            self.base_url, self.token,
            subscriptions or ["navigation", "navigation-validity",
                              "route-catalogue", "active-route", "readiness",
                              "planning-job", "environmental-job",
                              "environmental-dataset"],
            verify_tls=self.verify_tls, timeout=self.timeout)
