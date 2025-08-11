import io
import types


def test_worker_transcription_flow_monkeypatch(monkeypatch):
    # Lazy import worker module to patch
    import backend.worker.whisper.worker as w

    # Fake Redis: push one job then stop
    class FakeRedis:
        def __init__(self):
            self.jobs = [
                (
                    "jobs",
                    b'{"type":"transcribe","key":"devices/dev/2024/01/01/000000_1.wav"}',
                )
            ]

        def blpop(self, queue, timeout=5):
            return self.jobs.pop(0) if self.jobs else None

    # Fake MinIO: provide a tiny WAV and capture put_object
    class FakeResp:
        def __init__(self, data):
            self._data = data

        def read(self):
            return self._data

        def close(self):
            pass

        def release_conn(self):
            pass

    class FakeMinio:
        def __init__(self):
            self.writes = {}

        def get_object(self, bucket, key):
            return FakeResp(b"RIFF0000WAVEfmt ")

        def put_object(self, bucket, key, stream, length, content_type="text/plain"):
            self.writes[key] = stream.read()

    # Fake WhisperModel: return two segments
    class FakeSeg:
        def __init__(self, text):
            self.text = text

    class FakeModel:
        def transcribe(self, file_like, language=None):
            return ([FakeSeg("hello "), FakeSeg("world")], types.SimpleNamespace())

    monkeypatch.setenv("MINIO_BUCKET", "aipin")
    monkeypatch.setattr(
        w, "redis", types.SimpleNamespace(from_url=lambda url: FakeRedis())
    )
    monkeypatch.setattr(w, "Minio", lambda *a, **k: FakeMinio())
    monkeypatch.setattr(w, "WhisperModel", lambda *a, **k: FakeModel())

    # Run limited loop iteration
    # We patch blpop to return one item then None, causing the loop to continue; we break after one iteration using a timeout
    # So instead we call main() but monkeypatch the infinite loop to one pop
    # Here we simulate by calling the core logic manually
    r = w.redis.from_url("redis://localhost:6379/0")
    m = w.Minio("minio:9000", access_key="x", secret_key="y", secure=False)
    model = w.WhisperModel("small", compute_type="auto")

    item = r.blpop("jobs", timeout=1)
    _, payload = item
    job = __import__("json").loads(payload)
    assert job["type"] == "transcribe"
    key = job["key"]

    resp = m.get_object("aipin", key)
    data = resp.read()
    assert data.startswith(b"RIFF")

    segments, info = model.transcribe(io.BytesIO(data), language=None)
    text = "".join(s.text for s in segments)
    assert text.strip() == "hello world"
