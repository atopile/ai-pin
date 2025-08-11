import os
from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    api_host: str = os.getenv("API_HOST", "0.0.0.0")
    api_port: int = int(os.getenv("API_PORT", "8080"))

    minio_endpoint: str = os.getenv("MINIO_ENDPOINT", "minio:9000")
    minio_access_key: str = os.getenv("MINIO_ACCESS_KEY", "minio")
    minio_secret_key: str = os.getenv("MINIO_SECRET_KEY", "minio12345")
    minio_secure: bool = os.getenv("MINIO_SECURE", "false").lower() == "true"
    minio_bucket: str = os.getenv("MINIO_BUCKET", "aipin")

    redis_url: str = os.getenv("REDIS_URL", "redis://redis:6379/0")
