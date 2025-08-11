## AI Pin — Your private, wearable memory

**Stop forgetting what matters.** AI Pin is a privacy‑first wearable that captures your conversations throughout the day, then—when you’re home—offloads to your own local server to transcribe and make everything searchable. Ask, “What did I promise Alex yesterday?” or “What did I get done today?” and get instant answers from your own life.

### Why this is different

- **AI‑designed hardware**: The PCB is designed using `atopile` (a declarative electronics DSL) and lives as code in `ai-pin.ato`.
- **Local‑first by default**: All storage, transcription, embeddings and search run on your own machine—no cloud or third‑party services.
- **End‑to‑end system**: Wearable capture → local ingestion → Whisper transcription → vector search → Q&A and daily summaries.

### What it does

- **Continuous or on‑demand capture** via an I2S MEMS mic, storing audio on microSD.
- **Nightly offload** over Wi‑Fi while charging to a local server (Docker Compose).
- **Local transcription** using Whisper (GPU if available, CPU fallback).
- **Search & recall** across transcripts with semantic + keyword search.
- **Daily digest** of your day’s conversations and key moments.

### Moments you’ll stop forgetting

- “What did I promise to follow up on in today’s meeting?”
- “Who did I meet at the event—and what did we agree to do next?”
- “Which book/podcast did Sam recommend at lunch?”
- “What were the price, dates, and address the contractor mentioned?”
- “Where did we leave off on that debugging thread last week?”
- “What did the coach say about pickup changes for tomorrow?”
- “What’s the Wi‑Fi password/door code they told me?”
- “What exact phrasing did I use when I committed to that deliverable?”
- “What were the three ideas I said I’d write down later?”

---

## Architecture

```mermaid
graph TD
  Wearable["ESP32‑C3 AI Pin<br/>I2S mic → microSD (WAV)"] -->|Wi‑Fi offload| API[FastAPI Ingestion]
  API --> MINIO[(MinIO Object Storage)]
  API --> REDIS[(Redis Queue)]
  REDIS --> WORKER[Whisper Worker]
  MINIO --> WORKER
  WORKER --> MINIO
  WORKER --> DB[(Postgres + pgvector)]
  UI[Search & Daily Digest UI] --> API
  API --> DB
```

### Hardware (designed in code)

- MCU: ESP32‑C3 Mini‑1 (`I2S`, `SPI`, `Wi‑Fi`)
- Mic: TDK InvenSense ICS‑43434 (digital I2S)
- Storage: microSD over SPI
- Power: USB‑C → BQ25185 charger/power‑path → LDO (3.3 V)
- Battery: LP402535 (configurable)

See `ai-pin.ato` for the complete, declarative hardware design.

### Firmware (Zephyr RTOS)

- I2S capture → ring buffer → WAV writer (5–10 min segments)
- FATFS on SPI microSD, crash‑safe finalize, sidecar metadata
- Wi‑Fi offload window while charging; TLS upload
- Simple UX: LED states and a single button

### Local backend (Docker Compose)

- Ingestion API (FastAPI)
- Object storage (MinIO)
- Database (Postgres + pgvector)
- Queue (Redis)
- Transcription worker (Whisper via `faster-whisper`)

---

## Quickstart

### 1) Start the local stack

Create `backend/compose/.env` with these defaults (edit as needed):

```env
API_HOST=0.0.0.0
API_PORT=8080

MINIO_ENDPOINT=minio:9000
MINIO_ACCESS_KEY=minio
MINIO_SECRET_KEY=minio12345
MINIO_SECURE=false
MINIO_BUCKET=aipin

REDIS_URL=redis://redis:6379/0
WHISPER_MODEL=small
WHISPER_COMPUTE_TYPE=auto
```

Then launch:

```bash
cd backend/compose
docker compose up -d
```

Services:

- API: `http://localhost:8080`
- MinIO console: `http://localhost:9001` (user: `minio`, pass: `minio12345`)
- Postgres: `localhost:5432`
- Redis: `localhost:6379`

Health check:

```bash
curl http://localhost:8080/healthz
```

### 2) Upload a sample WAV (simulating the device)

```bash
curl -X POST \
  -F "device_id=test-device" \
  -F "file=@/path/to/sample.wav;type=audio/wav" \
  http://localhost:8080/v1/upload
```

The worker will pick it up, transcribe, and write a `.txt` next to the `.wav` in MinIO.

---

## Developing the firmware (ESP32‑C3 + Zephyr)

```bash
west build -b esp32c3_devkitm firmware/zephyr/app -p always
west flash
```

Notes:

- `prj.conf` enables I2S, SPI, FATFS, Wi‑Fi, Logging, Settings.
- Board overlay maps I2S mic, SPI microSD (CS GPIO4), LED, and button.

---

## Project status

- Hardware captured in `ai-pin.ato` (Atopile)
- Backend Compose stack scaffolded: API + MinIO + Postgres + Redis + Whisper worker
- Firmware stubs prepared (Zephyr app structure)

See `spec.md` and `todo.md` for detailed scope, milestones, and open questions.

---

## Roadmap (MVP highlights)

- End‑to‑end ingest → transcribe
- Daily summary generation
- OTA updates via MCUboot
- Optional SD encryption and mTLS device identity

---

## Contributing

Issues and PRs welcome. The goal is a great local‑first reference design for private, wearable memory.

---

## License

MIT
