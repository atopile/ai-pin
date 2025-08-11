# Backend – Local testing guide

Start services

```bash
cd /Users/narayanpowderly/projects/ai-pin/backend/compose
docker compose up -d --build
docker compose ps
docker compose logs -f api worker
```

Health check

```bash
curl http://localhost:8080/healthz
```

Prepare test audio

- Convert MP3 to 16 kHz mono WAV (recommended):

```bash
ffmpeg -i /path/to/song.mp3 -ar 16000 -ac 1 /tmp/test.wav
```

- Or generate a 5s tone:

```bash
ffmpeg -f lavfi -i "sine=frequency=1000:duration=5" -ar 16000 -ac 1 /tmp/test.wav
```

Upload file to API

- WAV:

```bash
curl -X POST \
  -F "file=@/tmp/test.wav;type=audio/wav" \
  -F "device_id=dev123" \
  http://localhost:8080/v1/upload
```

- MP3:

```bash
curl -X POST \
  -F "file=@/path/to/song.mp3;type=audio/mpeg" \
  -F "device_id=dev123" \
  http://localhost:8080/v1/upload
```

Watch transcription progress

```bash
docker compose logs -f worker
```

Inspect artifacts in MinIO

- Console: http://localhost:9001 (user: `minio`, pass: `minio12345`), bucket: `aipin`
- With AWS CLI:

```bash
export AWS_ACCESS_KEY_ID=minio
export AWS_SECRET_ACCESS_KEY=minio12345
aws --endpoint-url http://localhost:9000 s3 ls s3://aipin/ --recursive
# Download transcript (replace KEY):
aws --endpoint-url http://localhost:9000 s3 cp s3://aipin/KEY .
```

Troubleshooting

```bash
docker compose ps
docker compose logs api
docker compose restart api
```

- Re-run upload and tail worker logs:

```bash
docker compose logs -f worker
```

Stop services

```bash
docker compose down
```
