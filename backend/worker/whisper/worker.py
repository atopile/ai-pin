import os
import io
import json
import time
from minio import Minio
import redis

from faster_whisper import WhisperModel  # requires GPU libs if using GPU
import tempfile
import os


MINIO_ENDPOINT = os.getenv("MINIO_ENDPOINT", "minio:9000")
MINIO_ACCESS_KEY = os.getenv("MINIO_ACCESS_KEY", "minio")
MINIO_SECRET_KEY = os.getenv("MINIO_SECRET_KEY", "minio12345")
MINIO_SECURE = os.getenv("MINIO_SECURE", "false").lower() == "true"
MINIO_BUCKET = os.getenv("MINIO_BUCKET", "aipin")

REDIS_URL = os.getenv("REDIS_URL", "redis://redis:6379/0")
QUEUE = os.getenv("JOB_QUEUE", "jobs")

WHISPER_MODEL = os.getenv("WHISPER_MODEL", "small")
WHISPER_COMPUTE_TYPE = os.getenv("WHISPER_COMPUTE_TYPE", "auto")


def main():
    r = redis.from_url(REDIS_URL)
    m = Minio(
        MINIO_ENDPOINT,
        access_key=MINIO_ACCESS_KEY,
        secret_key=MINIO_SECRET_KEY,
        secure=MINIO_SECURE,
    )
    model = WhisperModel(WHISPER_MODEL, compute_type=WHISPER_COMPUTE_TYPE)

    print("whisper worker ready")
    while True:
        item = r.blpop(QUEUE, timeout=5)
        if not item:
            continue
        _, payload = item
        job = json.loads(payload)
        if job.get("type") != "transcribe":
            continue
        key = job["key"]
        print(f"processing {key}")
        # Download object to temp bytes
        response = m.get_object(MINIO_BUCKET, key)
        data = response.read()
        response.close()
        response.release_conn()

        # Transcribe (write to temp file so ffmpeg can decode mp3/wav reliably)
        with tempfile.NamedTemporaryFile(delete=False, suffix=".audio") as tmp:
            tmp.write(data)
            tmp_path = tmp.name
        try:
            segments, info = model.transcribe(tmp_path, language=None)
        except Exception as e:
            print(f"failed to transcribe {key}: {e}")
            segments = []
        finally:
            try:
                os.unlink(tmp_path)
            except OSError:
                pass
        text = "".join(s.text for s in segments)

        # Store transcript
        txt_key = key.replace(".wav", ".txt")
        if text:
            m.put_object(
                MINIO_BUCKET,
                txt_key,
                io.BytesIO(text.encode("utf-8")),
                length=len(text.encode("utf-8")),
                content_type="text/plain",
            )
            print(f"done {key} → {txt_key}")
        else:
            print(f"no transcript generated for {key}")


if __name__ == "__main__":
    main()
