import os
import asyncio
import websockets
from aiohttp import web

TXT_FILE = "data.txt"
MEM_FILE = "memory.txt"

connected = set()
last_modified = 0
mem_connected = set()
mem_last_modified = 0


def read_file():
    with open(TXT_FILE) as f:
        return f.read()


def read_memory():
    with open(MEM_FILE) as f:
        return f.read()


async def handler(websocket):
    connected.add(websocket)
    try:
        await websocket.send(read_file())
        async for _ in websocket:
            pass
    finally:
        connected.remove(websocket)


async def mem_handler(websocket):
    mem_connected.add(websocket)
    try:
        await websocket.send(read_memory())
        async for _ in websocket:
            pass
    finally:
        mem_connected.remove(websocket)


async def watch_file():
    global last_modified
    while True:
        try:
            modified = os.path.getmtime(TXT_FILE)
            if modified != last_modified:
                last_modified = modified
                data = read_file()
                if connected:
                    await asyncio.gather(*(ws.send(data) for ws in connected))
        except FileNotFoundError:
            pass
        await asyncio.sleep(1)


async def watch_memory():
    global mem_last_modified
    while True:
        try:
            modified = os.path.getmtime(MEM_FILE)
            if modified != mem_last_modified:
                mem_last_modified = modified
                data = read_memory()
                if mem_connected:
                    await asyncio.gather(*(ws.send(data) for ws in mem_connected))
        except FileNotFoundError:
            pass
        await asyncio.sleep(1)


async def post_data(request):
    plants = await request.json()
    p = plants[0]
    new = [str(p["ph"]), str(p["moisture"]), str(p["temp"])]

    with open(MEM_FILE) as f:
        lines = f.read().splitlines()
    while len(lines) < 3:
        lines.append(":".join(["0"] * 10))

    for i, value in enumerate(new):
        parts = lines[i].split(":")
        if len(parts) != 10:
            parts = ["0"] * 10
        lines[i] = ":".join(parts[1:] + [value])   # shift left, newest in column 10

    with open(MEM_FILE, "w") as f:
        f.write("\n".join(lines))
    print(f"ph={new[0]} moisture={new[1]} temp={new[2]}", flush=True)
    return web.Response(text="ok")


async def main():
    app = web.Application()
    app.add_routes([web.post("/data", post_data)])
    runner = web.AppRunner(app)
    await runner.setup()
    await web.TCPSite(runner, "0.0.0.0", 8080).start()

    async with websockets.serve(handler, "0.0.0.0", 8765), \
               websockets.serve(mem_handler, "0.0.0.0", 8766):
        await asyncio.gather(watch_file(), watch_memory())


if __name__ == "__main__":
    asyncio.run(main())