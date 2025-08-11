import io
import os
import types
import pytest
from fastapi.testclient import TestClient

from app.main import app


@pytest.fixture(autouse=True)
def patch_settings_env(monkeypatch):
    monkeypatch.setenv("MINIO_ENDPOINT", "localhost:9000")
    monkeypatch.setenv("MINIO_ACCESS_KEY", "minio")
    monkeypatch.setenv("MINIO_SECRET_KEY", "minio12345")
    monkeypatch.setenv("MINIO_SECURE", "false")
    monkeypatch.setenv("MINIO_BUCKET", "test-bucket")
    monkeypatch.setenv("REDIS_URL", "redis://localhost:6379/0")


class FakeMinio:
    def __init__(self):
        self.objects = {}
        self.bucket = None

    def bucket_exists(self, bucket):
        return True

    def make_bucket(self, bucket):
        self.bucket = bucket

    def put_object(
        self, bucket, key, stream, length, content_type="application/octet-stream"
    ):
        self.objects[key] = stream.read()


class FakeQueue:
    def __init__(self):
        self.jobs = []

    def enqueue(self, job: dict) -> bool:
        self.jobs.append(job)
        return True


def test_healthz():
    client = TestClient(app)
    resp = client.get("/healthz")
    assert resp.status_code == 200
    assert resp.json()["status"] == "ok"


def test_upload_success(monkeypatch):
    # Patch storage and queue
    from app import main as main_mod

    fake_minio = FakeMinio()
    monkeypatch.setattr(
        main_mod,
        "storage",
        types.SimpleNamespace(
            put_bytes=lambda k, b, content_type=None: fake_minio.put_object(
                "test-bucket", k, io.BytesIO(b), len(b), content_type
            )
        ),
    )
    fake_queue = FakeQueue()
    monkeypatch.setattr(main_mod, "queue", fake_queue)

    client = TestClient(app)
    wav_data = b"RIFF0000WAVEfmt "  # minimal header-like bytes
    files = {"file": ("test.wav", io.BytesIO(wav_data), "audio/wav")}
    data = {"device_id": "dev123", "ts": "20240101T000000"}
    resp = client.post("/v1/upload", files=files, data=data)
    assert resp.status_code == 200
    body = resp.json()
    assert body["size_bytes"] == len(wav_data)
    assert body["enqueued"] is True
    assert body["key"].endswith(".wav")


def test_upload_rejects_bad_content_type():
    client = TestClient(app)
    files = {"file": ("test.txt", io.BytesIO(b"abc"), "text/plain")}
    data = {"device_id": "dev123"}
    resp = client.post("/v1/upload", files=files, data=data)
    assert resp.status_code == 415
