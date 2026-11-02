from aiohttp import web
import asyncio
import websockets

connected = set()
TXT_FILE = "data.txt"
last_modified = 0


def read_file():
    with open(TXT_FILE) as f:
        return f.read()


async def handler(websocket):
    connected.add(websocket)
    try:
        await websocket.send(read_file())
        async for _ in websocket:
            pass 
    finally:
        connected.remove(websocket)


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

mem_connected = set()
MEM_FILE = "memory.txt"
mem_last_modified = 0


def read_memory():
    with open(MEM_FILE) as f:
        return f.read()


async def mem_handler(websocket):
    mem_connected.add(websocket)
    try:
        await websocket.send(read_memory())
        async for _ in websocket:
            pass
    finally:
        mem_connected.remove(websocket)


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

LABELS = ["PH", "Moisture", "Temperature", "Light", "Water"]
KEYS = ["ph", "moisture", "temp", "light", "water"]

async def post_data(request):
    plants = await request.json()  # [plant0, plant1]
    with open(MEM_FILE) as f:
        lines = f.read().splitlines()
    while len(lines) < 10:
        lines.append("")

    for i, plant in enumerate(plants[:2]):
        for k in range(5):
            r = i * 5 + k
            parts = lines[r].split(":")
            if len(parts) != 10:
                parts = [LABELS[k]] + ["0"] * 9
            value = str(plant[KEYS[k]])
            history = parts[1:][1:] + [value]          # shift history left, add newest
            first = value if (i == 0 and k == 0) else LABELS[k]
            lines[r] = ":".join([first] + history)

    with open(MEM_FILE, "w") as f:
        f.write("\n".join(lines))
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
    import os
    asyncio.run(main())