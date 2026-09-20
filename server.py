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


async def main():
    async with websockets.serve(handler, "0.0.0.0", 8765):
        await watch_file()


if __name__ == "__main__":
    import os
    asyncio.run(main())