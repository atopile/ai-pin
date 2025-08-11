## AI Pin – Product & Technical Specification

### Summary

- Wearable device that captures audio throughout the day, stores locally on microSD, and offloads via Wi‑Fi to a private, user‑hosted server while charging. The local server transcribes audio to text with Whisper and makes it searchable for questions like “What did I get done today?”.

### Goals

- Reliable audio capture with minimal user interaction
- Nightly/private offload and transcription
- Local‑first: all data and processing live on the user’s own machine/NAS/server (no cloud dependency)
- Searchable, privacy‑respecting archive with daily summaries

### Non‑Goals (initially)

- On‑device transcription (server‑side only)
- Cellular connectivity
- Complex display/UI (stick to LED/haptics/buttons)

---

## Hardware (current design per `ai-pin.ato`)

### Bill of Materials (logical)

- MCU/Radio: `ESP32_C3_MINI_1_driver` (ESP32‑C3 module)
- USB Power/Connector: `USB2_0TypeCHorizontalConnector`
- Charger/Power Path: `BQ25185_driver`
- Primary Regulators: `LDK220M_R` (LDO to 3.3 V)
- Battery: `EMB_BATTERY_LP402535_driver` (configured 300 mAh)
- Microphone: `TDK_InvenSense_ICS_43434` (digital I2S MEMS MIC)
- Storage: `MicroSDCardAssemblyWithRemovableSPI` (microSD over SPI, CS on `mcu.gpio[4]`)
- I2C: pull‑ups 10 kΩ (0402) to `power_3v3`

### Power Topology

- `usb_c.usb.usb_if.buspower` → `power_vbus_5v`
- `power_vbus_5v` → `charger.power_vbus`
- Battery rail: `battery.power` ↔ `power_batt` ↔ `charger.power_batt`
- System rail: `charger.power_sys` → `LDK220M_R` → `power_3v3` (asserted 2.97–3.15 V)
- Loads: `mcu.power`, `mic.power`, `microsd.power` tied to `power_3v3`

### Digital Interfaces

- Audio: `mcu.i2s ~ mic.i2s`
- Storage: `mcu.spi ~ microsd.spi`, `mcu.gpio[4] ~ microsd.spi_cs`
- I2C: pull‑ups to 3.3 V; `mcu.i2c.sda.reference.hv ~> R ~> mcu.i2c.sda.line` and same for SCL

### Notes & Risks

- ESP32‑C3 performance/pins are sufficient for SPI microSD + I2S mic recording to WAV; consider throughput testing and SD card class requirements.
- Unresolved package imports currently reported by the toolchain; see “Ato package checklist”.

### Ato package checklist (to install)

- `atopile/espressif-esp32-c3/esp32_c3_mini.ato`
- `atopile/usb-connectors/usb-connectors.ato`
- `atopile/st-ldk220/st-ldk220.ato`
- `atopile/microphones/tdk_invensense_ics_43434.ato`
- `atopile/ti-bq25185/ti-bq25185.ato`
- `atopile/sd-card/sd-card-slots.ato`
- `atopile/batteries/eemb_battery_lp402535.ato`

---

## Firmware (ESP32‑C3, Zephyr RTOS)

### Core Features

- Audio capture via Zephyr I2S API → ring buffer → WAV chunking
- File I/O to SPI microSD using Zephyr `fs` (FATFS) with safe rotation and metadata sidecars
- Wi‑Fi provisioning and nightly offload to a local server with TLS (mTLS optional)
- LED/haptics/button UX for recording, charging, syncing, errors
- Power management (charger state, low‑battery shutdown)
- OTA update via MCUboot (image management with mcumgr over BLE/Wi‑Fi or HTTP OTA)

### Concurrency Model

- Zephyr threads and work queues
- Audio thread: configure I2S, optional VAD, double‑buffer
- Storage thread: consume buffers, write `.wav`, fsync, rotate N minutes
- Uploader thread: Wi‑Fi connect, scan for pending files, resume, checksum verify
- Power management: charger/ADC polling, battery state, thermal guardrails
- Provisioning: BLE or SoftAP captive portal; credentials via Zephyr Settings subsystem
- LED/UI: state machine driving RGB LED patterns; debounced button via Zephyr input APIs

### File/Format

- WAV PCM mono, 16 kHz, 16‑bit (Whisper friendly)
- Segment duration: 5–10 min; filename includes RTC + monotonic seq
- Sidecar JSON per file: { timestamp range, dropped frames, battery %, gain, sw/hw versions }

### Reliability & Security

- Preallocation/atomic rename on close; journal index for recovery
- Backpressure policy: pause capture if SD stalls; telemetry counter
- TLS 1.2+ to server; optional per‑file encryption (ChaCha20‑Poly1305) with device key stored via Zephyr Settings (and optional hardware keying later)

### UX Mapping (proposed)

- Button: short press start/stop recording; long press enters provisioning/sync
- LED: recording (solid red), idle (dim cyan), uploading (blue pulse), charging (amber), full (green), error (red blink), mute (magenta)
- Optional haptic buzz on state change

---

## Offload & Backend (Local‑first)

### Ingestion

- Authenticated upload endpoint (token or mTLS) hosted on the user’s machine/NAS/server. Supports resumable uploads and metadata posting.
- Stores raw audio + sidecars to local object storage (MinIO):
  - `users/{userId}/devices/{deviceId}/{YYYY}/{MM}/{DD}/{HHmmss}_{seq}.wav`
  - matching `.json` and `.txt` (post‑transcription)

### Processing

- Local queue per uploaded file (e.g., Redis)
- Local transcription workers using Whisper (e.g., `faster-whisper`). Language auto‑detect; VAD prepass to skip silence. No cloud services required.
- Post‑processing: sentence boundary alignment, optional diarization, timestamps

### Storage & Search

- Local Postgres for metadata and transcripts (JSONB per chunk)
- Local pgvector for embeddings; alternatives: local Qdrant/Milvus if scale dictates
- RAG pipeline: hybrid search (BM25 + vector) with time filters and user constraints

### Query UX

- Daily digest: “What did I get done today?” generated from that day’s transcripts
- Free‑form Q&A over selected time ranges

### Security & Privacy (backend)

- LAN‑only by default (no Internet egress needed); optional remote access via user‑managed reverse proxy/VPN
- TLS everywhere; encrypted local volumes; MinIO with server‑side encryption; per‑user isolation
- Audit logging; retention controls; deletion handling
- Optional PII redaction pass on transcripts

### Local deployment

- Provide a `docker-compose.yml` that brings up: API (FastAPI/Express), MinIO, Postgres (+pgvector), Redis, and a Whisper worker (GPU‑accelerated if available; CPU fallback)
- mDNS for local discovery; device posts to `https://ai-pin.local` (configurable)

### Agents & Private History

Local agents can query transcripts to answer questions and automate tasks without leaving your machine.

#### Retrieval model

- Hybrid search: BM25 + vector with reciprocal rank fusion; time and device filters
- Chunking: sentence or ~512‑token windows with overlap; store timestamps per chunk
- Embeddings: local model (e.g., `bge-small-en`) stored in pgvector; optional Qdrant/Milvus at scale

#### Agent sessions & scopes

- Session object defines time window, people/topics (optional), device/user scope, and privacy level
- Responses include citations (file key, start/end timestamps) for traceability
- Redaction profile can be applied per session before returning text to agents

#### Agent API (planned)

- `GET /v1/search` – hybrid search over transcripts with filters and `top_k`
- `POST /v1/agents/query` – returns stitched context windows for LLMs plus citations
- `POST /v1/agents/sessions` – create session with scope and redaction policy; returns `session_id`

#### Data model additions (planned)

- `agent_sessions(id, user_id, created_at, scope_json, redaction_profile)`
- `search_logs(id, session_id?, query, filters_json, top_k, latency_ms, created_at)`

#### Privacy & security defaults

- Bind API to loopback or LAN; no egress by default
- PAT or mTLS for agent clients; per‑user isolation
- Optional PII redaction before indexing and/or at query time
- Retention policies and deletion propagate to vector store and MinIO

---

## Data Model (sketch)

### Objects

- Audio: `.wav` in object storage; sidecars `.json`; transcripts `.txt` or JSON

### Database (Postgres)

- `users(id, email, created_at, ...)`
- `devices(id, user_id, model, public_key?, created_at, ...)`
- `files(id, device_id, path, start_ts, end_ts, duration_s, size_bytes, checksum, status, created_at)`
- `transcript_chunks(id, file_id, start_ms, end_ms, text, confidence, speaker?)`
- `embeddings(id, chunk_id, vector, model, created_at)`
- `jobs(id, type, status, payload, created_at, updated_at)`

---

## Security & Provisioning

- Device identity: token provisioned via BLE/SoftAP portal; optional per‑device cert (mTLS)
- On‑device keys via Zephyr Settings; secure boot/flash encryption can be enabled in later revisions
- SD at‑rest encryption optional (configurable per user)

---

## Milestones

- v0 Bench Prototype
  - ESP32‑C3 dev board + I2S mic + SPI microSD
  - Record WAV, manual Wi‑Fi upload, basic server endpoint
- v1 MVP Wearable
  - Custom PCB (this design), nightly offload, server transcription, searchable UI, OTA
- v2 Reliability/Privacy
  - Encryption at rest, device provisioning flow, improved power telemetry
- v3 UX & ML
  - Advanced Q&A, diarization, mobile companion app

---

## Acceptance Criteria (MVP)

- 8+ hours of continuous recording on 300 mAh battery or clear segmented/on‑demand mode documented
- <1% dropped audio frames per hour on a Class 10 microSD (specified test card)
- Nightly offload completes for a full day’s data over WPA2 network with retry
- End‑to‑end transcription and searchable queries over last 7 days
- OTA update succeeds and is roll‑back safe

---

## Implementation Options & Trade‑offs

- Mic: I2S MEMS (simple, low BOM) vs analog mic + codec (higher fidelity, more power/complexity)
- SD: SPI (fewer pins, slower) vs SDMMC 4‑bit (faster, more pins)
- Encryption: on‑device per‑file (privacy, CPU/battery cost) vs server‑only
- Provisioning: BLE (mobile‑friendly) vs SoftAP (no app, clunkier UX)
- Vector store: pgvector (simple ops) vs Qdrant/Milvus (scale)

---

## Open Questions

### Hardware

- Keep ESP32‑C3 or upgrade to S3 for USB‑OTG and more I2S headroom?
- Confirm ICS‑43434 mic orientation/porting and mechanical acoustics (wind/noise filters)?
- SPI microSD performance: required write throughput, target SD class, acceptable latency spikes?
- Include a fuel gauge (e.g., MAX17048) for better battery UX, or estimate via voltage?
- Add hardware mute switch for mic? LED behavior when muted?
- Charging LED behaviors and power‑path current limits for record‑while‑charge?

### Firmware

- Continuous recording vs VAD‑triggered segments to save space/power?
- Segment length: 5 vs 10 minutes vs silence‑aligned segments?
- On‑device file encryption from day one or later?
- Provisioning method: BLE vs SoftAP; do we ship a mobile app?
- Minimum acceptable dropped‑frame rate under worst‑case SD latency?
- Exact LED/haptic state machine definitions and timeouts?

### Backend/ML

- Transcription: Whisper locally (`faster-whisper`, choose model size per hardware); target accuracy vs local compute constraints?
- Embeddings: local (`bge-small-en` or similar) vs hosted; default is local‑only
- Vector DB: pgvector vs Qdrant/Milvus; expected daily transcript volume and 1‑year scale?
- PII handling/redaction requirements and default retention window?
- Query UX: chat‑style vs daily digest first; which user interfaces (web/mobile)?

### Security/Operations

- Device auth: token vs mTLS; key rotation strategy?
- SD at‑rest encryption: key provisioning, backup, and device‑loss UX?
- Wi‑Fi credential rotation and multi‑SSID support?
- OTA cadence, staged rollout, and failure recovery policy?

---

## Test Plan (high level)

- Throughput: validate continuous write of 16‑kHz 16‑bit mono WAV with <1% drops over 8 h
- Power: measure battery life in continuous vs VAD modes; thermals while charging + recording
- Filesystem: abrupt power‑loss tests; index recovery; file integrity checksums
- Networking: upload retries, resume support, checksum verification; TLS certificate validation to local server
- Backend: local queue backlog handling, transcription latency, accuracy sampling; measure CPU/GPU utilization
- Search: relevance on benchmark queries; time‑range filters; daily digest quality

---

## Next Steps

- Resolve Ato package imports and build PCB draft (hardware is largely done)
- Scaffold Zephyr application (I2S capture, FATFS on SPI SD, Wi‑Fi manager, TLS uploader, LED/button, Zephyr Settings)
- Create `docker-compose.yml` for local stack (API, MinIO, Postgres+pgvector, Redis, Whisper worker)
- Implement ingestion API and local Whisper worker
- Minimal web UI for daily summaries and free‑form questions
