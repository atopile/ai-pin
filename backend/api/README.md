# Backend API

Local-first ingestion and query API.

Run

- This service is started via `backend/compose/docker-compose.yml`.
- Exposes `http://localhost:8080`.

Endpoints (planned)

- POST `/v1/upload` – resumable audio upload + metadata
- GET `/v1/files` – list uploaded files
- GET `/v1/search` – semantic search over transcripts
- POST `/v1/agents/query` – stitched context windows + citations for LLMs
- POST `/v1/agents/sessions` – create a scoped agent session
- GET `/healthz` – health check
