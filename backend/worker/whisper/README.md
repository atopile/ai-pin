# Whisper Worker

Runs local Whisper (e.g., faster-whisper) to transcribe uploaded audio from MinIO and write results back.

Planned flow

- Watch queue (Redis) for new file events
- Download WAV from MinIO
- Transcribe with configured Whisper model
- Store transcript (.txt/.json) and push chunks+embeddings to DB
