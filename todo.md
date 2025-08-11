## TODO – AI Pin (Zephyr firmware, local-first backend)

### MVP

#### Firmware (Zephyr, ESP32‑C3)

- [ ] Project scaffold
  - [ ] Create Zephyr app skeleton (board: ESP32‑C3; overlays for pins)
  - [ ] `prj.conf`: enable I2S, SPI, FATFS, Wi‑Fi, HTTP/TLS, Settings, Logging, MCUboot
  - [ ] DTS overlay: I2S mic, SPI microSD (CS on GPIO4), LED, button
- [ ] Audio capture & buffering
  - [ ] Configure I2S RX mono 16 kHz, 16‑bit
  - [ ] Ring buffer with drop counters; backpressure policy
  - [ ] WAV writer (RIFF headers, atomic finalize); 5–10 min rotation
- [ ] Sidecar JSON: timestamps, drops, versions
- [ ] Storage (microSD over SPI)
  - [ ] Mount FATFS; performance test with target SD class
  - [ ] Preallocation, fsync cadence; crash‑safe rename on close
  - [ ] Journaled file index and recovery on boot
- [ ] Networking & offload
  - [ ] Wi‑Fi manager (auto connect, backoff); NTP sync
- [ ] TLS uploader with token auth
  - [ ] Resumable/chunked uploads; checksum verification; retry/resume
  - [ ] SoftAP or BLE provisioning; persist creds via Settings
- [ ] UI & power
  - [ ] LED state machine (recording/idle/uploading/error)
  - [ ] Button handling (debounce; short/long press; provisioning)
  - [ ] Charger/fuel telemetry (ADC/charger IC); low‑battery shutdown
- [ ] OTA
  - [ ] Enable MCUboot; signed image update; rollback support
- [ ] Telemetry & logs
  - [ ] Persist counters

#### Backend (Local‑first; Docker Compose)

- [ ] Security
  - [ ] Optional per‑file SD encryption (ChaCha20‑Poly1305); key storage in Settings
  - [ ] Key rotation/wipe flow
- [ ] Telemetry & logs
  - [ ] Persist counters; optional log upload during offload window

### Backend (Local‑first; Docker Compose)

- [ ] Compose stack
  - [ ] Services: API, MinIO, Postgres(+pgvector), Redis, Whisper worker, optional Caddy (TLS)
  - [ ] Volumes, networks, `.env.sample` and defaults
- [ ] Ingestion API
  - [ ] Device registration + auth (PAT)
  - [ ] Resumable file uploads; checksum; metadata endpoint
  - [ ] mDNS discovery; LAN‑only default
- [ ] Processing
  - [ ] Whisper worker (faster‑whisper), CPU fallback (GPU if available)
  - [ ] Store transcript artifacts (.txt/.json)
- [ ] Storage & indexing
  - [ ] DB schema + migrations (users, devices, files, transcript_chunks, embeddings, jobs)
  - [ ] Embeddings pipeline (local model, e.g., bge‑small‑en); pgvector index
  - [ ] Background indexer and compaction
- [ ] Search & summaries
  - [ ] Query API (BM25+vector, time filters)
  - [ ] Minimal web UI (upload status, search, daily summary)
- [ ] Admin & ops
  - [ ] Metrics/health endpoints; dashboards
  - [ ] Retention/purge tools; backup scripts for MinIO/Postgres
  - [ ] TLS: self‑signed or Caddy-managed certs; device trust bootstrap

#### Testing & Validation (MVP)

- [ ] Firmware throughput/soak: 8 h continuous write, <1% drops on target SD
- [ ] Filesystem robustness: abrupt power loss, index recovery, integrity checksums
- [ ] Networking: retries, resume, TLS validation to local server
- [ ] Backend: queue handling, transcription latency, CPU utilization
- [ ] End‑to‑end: device → local API → transcription → search

##### Firmware Testing Plan (Zephyr, ESP32‑C3)

- [ ] Prereqs & setup

  - [ ] Hardware: ESP32‑C3 DevKitM (or your board), formatted microSD (FAT32), I2S MEMS mic wired per `esp32c3*.overlay`.
  - [ ] Create `wifi.txt` on SD root:
    ```ini
    ssid=YOUR_SSID
    psk=YOUR_PASSWORD
    ```
  - [ ] Connect UART and prepare a serial monitor (115200‑8‑N‑1). Example:
    ```bash
    screen /dev/tty.usbserial* 115200
    ```

- [ ] Build & flash smoke test

  - [ ] Build and flash:
    ```bash
    west build -b esp32c3_devkitm firmware/zephyr/app -p always
    west flash
    ```
  - [ ] On boot, confirm logs show: "AI Pin firmware booting" with no immediate errors.

- [ ] Audio capture (I2S → ring buffer)

  - [ ] Verify I2S config succeeds (no `i2s_configure` / `i2s start failed` errors).
  - [ ] Observe for `Audio ring overflow` warnings during silence and speech; occasional bursts are OK during bring‑up.
  - [ ] Acceptance: no persistent I2S errors; overflow warning rate low at idle; audio thread stays running.

- [ ] Storage (FATFS on SPI SD) and WAV correctness

  - [ ] Confirm FAT mounts (`Mounted FAT at /SD:` in logs). If SD is absent, insert and reboot.
  - [ ] Confirm `.wav` files appear in SD root; filenames are monotonic (ms timestamp).
  - [ ] Copy a segment to host and validate header:
    ```bash
    ffprobe -hide_banner -show_streams -select_streams a:0 /path/to/file.wav | cat
    ```
    - Expect PCM S16LE, 1 channel, 16000 Hz; duration ≈ segment length.
  - [ ] Segment rotation: temporarily set `segment_seconds = 10` (dev build) and confirm new file every 10 s.
  - [ ] Flush cadence: verify file grows and is synced roughly once per second (no massive loss after reset).
  - [ ] Crash‑safety: power‑cycle mid‑record; after reboot, last completed file remains valid; current file may be partial but header of previous file remains correct.

- [ ] Throughput & soak (target: <1% drops over 8 h)

  - [ ] Let device run 30–60 min, then 8 h, capturing UART logs to a file.
  - [ ] Compute drop ratio from logs: `sum(dropped_bytes) / total_bytes_written`.
    - Total bytes per second expected: `16000 * 1 * (16/8) = 32000 B/s`.
    - Acceptance: drop ratio < 1% over 8 h on target SD class.

- [ ] Wi‑Fi connect (credentials from SD)

  - [ ] Ensure `/SD:/wifi.txt` present; observe logs: `Read Wi‑Fi credentials ...`, then `Got IPv4 address` and `Wi‑Fi connected and IPv4 acquired` within ~20 s.
  - [ ] Negative: wrong password → `Wi‑Fi connect timeout` is logged; fix creds and confirm recovery after reboot.

- [ ] Optional diagnostics (when needed)

  - [ ] Enable Zephyr stack usage tracking to verify `MAIN_STACK_SIZE`, worker stacks are sufficient.
  - [ ] Add a compile‑time option to print bytes/sec written and ring buffer depth watermark.

- [ ] Firmware acceptance criteria
  - [ ] Device builds, flashes, and boots without errors.
  - [ ] WAV segments are valid PCM S16LE @ 16 kHz; rotation works at configured interval.
  - [ ] Crash during write does not corrupt previously finalized segments.
  - [ ] Over an 8 h soak, ring overflow implied drops < 1% (based on logs and file sizes).
  - [ ] Wi‑Fi connects with credentials from SD and reaches DHCP/IPv4 assignment.

##### Backend Testing Plan (API + Worker + MinIO + Redis)

- [ ] Automated unit tests

  - [ ] API: run `pytest -q` in `backend/api` (covers `/healthz`, upload happy‑path, and content‑type rejection)
  - [ ] Worker: run `pytest -q` in `backend/worker/whisper` (covers queue pop, MinIO read/write, and transcription glue)

- [ ] Integration tests (Docker Compose)

  - [ ] Start stack
    - [ ] `cd backend/compose && docker compose up -d`
    - [ ] Ensure `.env` matches defaults in `README.md` (`MINIO_*`, `REDIS_URL`, `WHISPER_*`).
  - [ ] Health checks
    - [ ] API: `curl http://localhost:8080/healthz` → `{ "status": "ok" }`
    - [ ] MinIO console reachable at `http://localhost:9001` (login: `minio` / `minio12345`)
    - [ ] Redis reachable at `localhost:6379`
  - [ ] Upload sample WAV (simulated device)
    - [ ] Create a 2s mono 16kHz WAV:
      ```bash
      ffmpeg -f lavfi -i anullsrc=channel_layout=mono:sample_rate=16000 -t 2 -c:a pcm_s16le sample.wav
      ```
    - [ ] Upload:
      ```bash
      curl -X POST \
        -F "device_id=test-device" \
        -F "file=@./sample.wav;type=audio/wav" \
        http://localhost:8080/v1/upload
      ```
    - [ ] Assert response includes `key`, `size_bytes > 0`, `enqueued: true`.
  - [ ] Verify object landed in MinIO
    - [ ] Via console: bucket `aipin` contains the uploaded `devices/test-device/YYYY/MM/DD/...wav` key.
  - [ ] Verify worker transcription
    - [ ] `docker compose logs -f worker | sed -n 's/.*done .* → .*\.txt.*/&/p' | head -n 1 | cat` shows completion for the key
    - [ ] MinIO contains adjacent `.txt` next to the uploaded `.wav` key; file non‑empty.

- [ ] Negative/edge cases

  - [ ] Unsupported content‑type: upload `text/plain` → API returns `415` (covered by tests)
  - [ ] Empty file: upload zero‑byte WAV → API accepts, MinIO stores, worker produces empty/near‑empty `.txt` (define expected behavior)
  - [ ] Large file (e.g., 50–100 MB): upload succeeds without API timeout; worker processes within reasonable time
  - [ ] Redis unavailable: simulate by stopping `redis`; upload should currently error (500). Capture and note expected behavior for future graceful‑degradation.
  - [ ] MinIO unavailable: simulate by stopping `minio`; upload should error (500). Confirm API surfaces failure clearly.

- [ ] Basic performance checks

  - [ ] Burst upload 10×2s WAVs (looped curl) → all enqueued; queue drains to zero within N seconds; `.txt` objects present for all keys
  - [ ] Measure end‑to‑end latency (upload → `.txt`) under idle conditions; record baseline

- [ ] Acceptance criteria
  - [ ] With the compose stack up, a WAV upload returns 200 and enqueues a job
  - [ ] The worker writes a non‑empty `.txt` next to the `.wav` in MinIO
  - [ ] Negative tests behave as specified above (415 for bad content‑type; clear 5xx on backend dependency failure)
  - [ ] All unit tests pass in both API and worker

### Later

#### Security & Privacy

- [ ] Threat model and data‑flow diagram
- [ ] Local‑only default; guidance for optional remote access (VPN/reverse proxy)
- [ ] Data export/delete; per‑user isolation; audit logging
- [ ] SD at‑rest encryption option; key provisioning and recovery plan

#### Additional Testing

- [ ] Power tests: battery life (continuous vs VAD), thermals while charging+recording
- [ ] Search relevance benchmarks; daily digest quality sampling

### Milestones

- [ ] v0 Bench Prototype
  - [ ] Record WAV to SD; manual upload to local API; server stores file
  - [ ] Manual transcription via worker script
- [ ] v1 MVP Wearable
  - [ ] Auto nightly upload; Whisper transcription; search UI; OTA
  - [ ] LED/button UX; provisioning; NTP; checksum & resume
- [ ] v2 Reliability/Privacy
  - [ ] SD encryption option; metrics/observability; improved battery UX
  - [ ] Signed updates; key rotation; retention tools

### Decisions Needed

- [ ] Provisioning method: BLE vs SoftAP (default?)
- [ ] SD encryption on by default? Key backup UX?
- [ ] Whisper model size(s) for worker (CPU‑only vs GPU‑capable)
- [ ] Embedding model (local) and vector store (pgvector vs Qdrant)
