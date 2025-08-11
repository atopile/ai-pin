import json
import redis


class JobQueue:
    def __init__(self, url: str, queue_name: str = "jobs"):
        self.r = redis.from_url(url)
        self.queue = queue_name

    def enqueue(self, job: dict) -> bool:
        self.r.rpush(self.queue, json.dumps(job))
        return True

    def dequeue(self, block: bool = True, timeout: int = 5):
        if block:
            item = self.r.blpop(self.queue, timeout=timeout)
            if item is None:
                return None
            _, payload = item
        else:
            payload = self.r.lpop(self.queue)
            if payload is None:
                return None
        return json.loads(payload)
