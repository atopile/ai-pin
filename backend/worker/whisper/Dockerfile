FROM python:3.11-slim

WORKDIR /app

ENV PIP_NO_CACHE_DIR=1

RUN apt-get update && apt-get install -y ffmpeg && rm -rf /var/lib/apt/lists/*

COPY requirements.txt /app/requirements.txt
RUN pip install --upgrade pip && pip install -r requirements.txt

COPY worker.py /app/worker.py

CMD ["python", "-u", "worker.py"]

