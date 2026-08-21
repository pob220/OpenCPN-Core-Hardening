import io
import json
import os
import sys
import unittest
import urllib.error
from unittest.mock import patch

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "src"))
from opencpn_control import Client, OpenCPNError


class Response:
    def __init__(self, value):
        self.data = json.dumps(value).encode()
    def __enter__(self): return self
    def __exit__(self, *_): return None
    def read(self): return self.data


class Transport:
    def __init__(self): self.requests = []
    def __call__(self, request, **_):
        self.requests.append(request)
        if request.headers.get("Authorization") != "Bearer secret":
            body = io.BytesIO(json.dumps(
                {"error": {"code": "invalid_token", "message": "bad"}}).encode())
            raise urllib.error.HTTPError(request.full_url, 401, "Unauthorized", {}, body)
        path = request.full_url.removeprefix("https://test.invalid")
        if path == "/api/v2/version": return Response({"apiVersion": "2.0.0"})
        if path == "/api/v2/readiness": return Response({"ready": True})
        if path == "/api/v2/capabilities": return Response({"capabilities": ["routes.query.v1"]})
        if path == "/api/v2/routes": return Response({"routes": []})
        if path == "/api/v2/providers":
            return Response({"providers": [{"capability": "environmental-data.test.v1"}]})
        if path == "/api/v2/environment/datasets":
            return Response({"datasets": [{"identity": "dataset-1"}]})
        if path == "/api/v2/planning/jobs":
            return Response({"id": "job-1", "state": "queued"})
        if path == "/api/v2/environment/jobs":
            return Response({"id": "environment-1", "state": "queued"})
        if path == "/api/v2/environment/datasets/dataset-1/activate":
            return Response({"identity": "dataset-1", "active": True})
        raise AssertionError(f"unexpected request: {request.method} {path}")


class ClientTest(unittest.TestCase):
    def setUp(self):
        self.transport = Transport()
        self.patch = patch("urllib.request.urlopen", self.transport)
        self.patch.start()
    def tearDown(self): self.patch.stop()

    def test_status_and_routes(self):
        client = Client("https://test.invalid", "secret")
        self.assertTrue(client.status()["readiness"]["ready"])
        self.assertEqual(client.list_routes(), [])

    def test_structured_error_redacts_token(self):
        with self.assertRaises(OpenCPNError) as caught:
            Client("https://test.invalid", "wrong").version()
        self.assertEqual(caught.exception.code, "invalid_token")
        self.assertNotIn("wrong", str(caught.exception))

    def test_planning_submission_has_idempotency(self):
        result = Client("https://test.invalid", "secret").start_plan(
            {"providerCapability": "test.v1"})
        self.assertEqual(result["id"], "job-1")
        self.assertTrue(self.transport.requests[-1].headers.get("Idempotency-key"))

    def test_provider_and_environment_lifecycle(self):
        client = Client("https://test.invalid", "secret")
        self.assertEqual(client.providers()[0]["capability"],
                         "environmental-data.test.v1")
        result = client.start_environment(
            {"providerCapability": "environmental-data.test.v1", "parameters": {}})
        self.assertEqual(result["id"], "environment-1")
        self.assertTrue(self.transport.requests[-1].headers.get("Idempotency-key"))
        self.assertEqual(client.list_environment_datasets()[0]["identity"], "dataset-1")
        self.assertTrue(client.activate_environment_dataset("dataset-1")["active"])


if __name__ == "__main__": unittest.main()
