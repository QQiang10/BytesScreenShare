"""
benchmark.py

A simple benchmark script for WebRTC signaling server peak REGISTER throughput.

Requirements:
    pip install websockets

Usage:
    python benchmark_register_peak.py

This script will continuously connect to ws://127.0.0.1:11290,
    send REGISTER_REQUEST, wait for REGISTER_SUCCESS, disconnect, and repeat.
    It prints QPS and latency stats every second.
"""
import asyncio
import websockets
import time
import statistics
import json

SERVER_URL = "ws://127.0.0.1:11290"

REGISTER_REQUEST = {
    "type": "REGISTER_REQUEST",
    "to": "Server",
    "data": {}
}

class Stats:
    def __init__(self):
        self.latencies = []
        self.count = 0
        self.start_time = time.time()
        self.last_report = self.start_time
        self.last_count = 0

    def record(self, latency):
        self.latencies.append(latency)
        self.count += 1

    def report(self):
        now = time.time()
        elapsed = now - self.last_report
        if elapsed < 1.0:
            return None
        window_count = self.count - self.last_count
        self.last_count = self.count
        self.last_report = now
        if window_count == 0:
            return "No responses in last %.2fs" % elapsed
        window_latencies = self.latencies[-window_count:] if window_count > 0 else []
        avg = statistics.mean(window_latencies) if window_latencies else 0
        p95 = statistics.quantiles(window_latencies, n=20)[18] if len(window_latencies) >= 20 else avg
        minl = min(window_latencies) if window_latencies else 0
        maxl = max(window_latencies) if window_latencies else 0
        return f"QPS: {window_count/elapsed:.1f}, avg: {avg*1000:.1f}ms, min: {minl*1000:.1f}ms, max: {maxl*1000:.1f}ms, p95: {p95*1000:.1f}ms"

async def run_benchmark():
    stats = Stats()
    async def single_cycle():
        try:
            async with websockets.connect(SERVER_URL, max_queue=1) as ws:
                t0 = time.time()
                await ws.send(json.dumps(REGISTER_REQUEST))
                while True:
                    msg = await ws.recv()
                    t1 = time.time()
                    try:
                        data = json.loads(msg)
                    except Exception:
                        continue
                    if data.get("type") == "REGISTER_SUCCESS":
                        stats.record(t1 - t0)
                        break
        except Exception:
            pass
    async def reporter():
        while True:
            await asyncio.sleep(1)
            r = stats.report()
            if r:
                print(r)
    # Launch reporter
    asyncio.create_task(reporter())
    # Main loop: run as fast as possible
    while True:
        await single_cycle()

if __name__ == "__main__":
    asyncio.run(run_benchmark())
