import os
import io
import json
import time
from datetime import datetime
from typing import Optional

from fastapi import FastAPI, UploadFile, File, Form, HTTPException
from fastapi.responses import JSONResponse
from pydantic import BaseModel

from .storage import MinioClient
from .queue import JobQueue
from .config import Settings


app = FastAPI(title="AI Pin API", version="0.1.0")

settings = Settings()
storage = MinioClient(
    endpoint=settings.minio_endpoint,
    access_key=settings.minio_access_key,
    secret_key=settings.minio_secret_key,
    secure=settings.minio_secure,
    bucket=settings.minio_bucket,
)
queue = JobQueue(url=settings.redis_url)


class UploadResponse(BaseModel):
    key: str
    size_bytes: int
    enqueued: bool


@app.get("/healthz")
def healthz():
    return {"status": "ok"}


@app.post("/v1/upload", response_model=UploadResponse)
async def upload(
    file: UploadFile = File(...),
    device_id: str = Form(...),
    ts: Optional[str] = Form(None),
):
    if file.content_type not in (
        "audio/wav",
        "audio/x-wav",
        "audio/mpeg",
        "audio/mp3",
        "application/octet-stream",
    ):
        raise HTTPException(status_code=415, detail="Unsupported content type")

    # Build key: users/<device>/YYYY/MM/DD/HHMMSS_seq.wav (user scope optional later)
    now = datetime.utcnow()
    ts_str = ts or now.strftime("%Y%m%dT%H%M%S")
    base_prefix = f"devices/{device_id}/{now.strftime('%Y/%m/%d')}"
    # Read file into memory or stream to MinIO
    data = await file.read()
    size_bytes = len(data)
    seq = int(time.time() * 1000) % 1000000
    key = f"{base_prefix}/{ts_str}_{seq}.wav"

    storage.put_bytes(key, data, content_type=file.content_type)

    # Enqueue job for transcription
    job = {
        "type": "transcribe",
        "key": key,
        "device_id": device_id,
        "content_type": file.content_type,
        "size_bytes": size_bytes,
        "uploaded_at": now.isoformat() + "Z",
    }
    enqueued = queue.enqueue(job)

    return UploadResponse(key=key, size_bytes=size_bytes, enqueued=enqueued)
