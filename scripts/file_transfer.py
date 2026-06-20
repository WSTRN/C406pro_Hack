#!/usr/bin/env python3
"""Transfer files to and from the C406Pro LittleFS partition over BLE.

Install the only dependency with: python3 -m pip install bleak
"""

import argparse
import asyncio
import shlex
import struct
from pathlib import Path
from typing import Optional

from bleak import BleakClient, BleakScanner
from bleak.exc import BleakError


DEVICE_NAME = "C406Pro_Hack"
CONTROL_UUID = "8c7a0002-5b7d-4f6c-9a2e-3d4b6f8a1000"
DATA_UUID = "8c7a0003-5b7d-4f6c-9a2e-3d4b6f8a1000"

CMD_LIST = 1
CMD_UPLOAD_BEGIN = 2
CMD_UPLOAD_END = 3
CMD_DOWNLOAD = 4
CMD_DELETE = 5

RSP_LIST_ITEM = 0x81
RSP_LIST_DONE = 0x82
RSP_UPLOAD_READY = 0x83
RSP_UPLOAD_DONE = 0x84
RSP_DOWNLOAD_INFO = 0x85
RSP_DOWNLOAD_DONE = 0x86
RSP_DELETE_DONE = 0x87
RSP_UPLOAD_CHUNK = 0x88


class FileClient:
    def __init__(self, client: BleakClient):
        self.client = client
        self.control = asyncio.Queue()
        self.download_data = bytearray()

    async def start(self):
        await self.client.start_notify(CONTROL_UUID, self._on_control)
        await self.client.start_notify(DATA_UUID, self._on_data)

    def _on_control(self, _characteristic, data: bytearray):
        self.control.put_nowait(bytes(data))

    def _on_data(self, _characteristic, data: bytearray):
        self.download_data.extend(data)

    async def _wait(self, response: int) -> bytes:
        while True:
            packet = await asyncio.wait_for(self.control.get(), timeout=30)
            if packet[0] != response:
                continue
            status = packet[1]
            if status:
                raise RuntimeError(f"device error: -{status}")
            return packet[2:]

    async def list(self):
        await self.client.write_gatt_char(CONTROL_UUID, bytes([CMD_LIST]), response=True)
        while True:
            packet = await asyncio.wait_for(self.control.get(), timeout=30)
            status = packet[1]
            if status:
                raise RuntimeError(f"device error: -{status}")
            if packet[0] == RSP_LIST_DONE:
                return
            if packet[0] == RSP_LIST_ITEM:
                size = struct.unpack_from("<I", packet, 2)[0]
                print(f"{packet[6:].decode():<40} {size:>10} bytes")

    async def upload(self, local: Path, remote_name: str):
        size = local.stat().st_size
        name = remote_name.encode()
        await self.client.write_gatt_char(
            CONTROL_UUID,
            bytes([CMD_UPLOAD_BEGIN]) + struct.pack("<I", size) + name,
            response=True,
        )
        await self._wait(RSP_UPLOAD_READY)

        characteristic = self.client.services.get_characteristic(DATA_UUID)
        chunk_size = characteristic.max_write_without_response_size
        sent = 0
        with local.open("rb") as source:
            while chunk := source.read(chunk_size):
                await self.client.write_gatt_char(DATA_UUID, chunk, response=False)
                payload = await self._wait(RSP_UPLOAD_CHUNK)
                sent += len(chunk)
                received = struct.unpack("<I", payload)[0]
                if received != sent:
                    raise RuntimeError(f"device acknowledged {received} of {sent} bytes")
                if sent % (chunk_size * 16) < chunk_size:
                    print(f"\rput {sent}/{size} bytes", end="", flush=True)
                    await asyncio.sleep(0.01)
        if size:
            print()

        await self.client.write_gatt_char(CONTROL_UUID, bytes([CMD_UPLOAD_END]), response=True)
        payload = await self._wait(RSP_UPLOAD_DONE)
        received = struct.unpack("<I", payload)[0]
        print(f"uploaded {received} bytes as {remote_name}")

    async def download(self, remote_name: str, local: Path):
        self.download_data.clear()
        await self.client.write_gatt_char(
            CONTROL_UUID, bytes([CMD_DOWNLOAD]) + remote_name.encode(), response=True
        )
        payload = await self._wait(RSP_DOWNLOAD_INFO)
        expected = struct.unpack("<I", payload)[0]
        await self._wait(RSP_DOWNLOAD_DONE)
        if len(self.download_data) != expected:
            raise RuntimeError(
                f"short download: received {len(self.download_data)} of {expected} bytes"
            )
        local.write_bytes(self.download_data)
        print(f"downloaded {expected} bytes to {local}")

    async def delete(self, remote_name: str):
        await self.client.write_gatt_char(
            CONTROL_UUID, bytes([CMD_DELETE]) + remote_name.encode(), response=True
        )
        await self._wait(RSP_DELETE_DONE)
        print(f"deleted {remote_name}")

class InteractiveShell:
    def __init__(self, files: FileClient):
        self.files = files
        self.local_cwd = Path.cwd()

    def local_path(self, name: str) -> Path:
        path = Path(name).expanduser()
        return path if path.is_absolute() else self.local_cwd / path

    async def run(self):
        print(f"Connected to {DEVICE_NAME}. Type 'help' for commands.")
        while self.files.client.is_connected:
            try:
                line = await asyncio.to_thread(input, f"{DEVICE_NAME}:/lfs> ")
                args = shlex.split(line)
                if not args:
                    continue
                command = args[0].lower()
                if command in ("quit", "exit", "bye"):
                    return
                if command in ("help", "?"):
                    self.help()
                elif command in ("ls", "dir"):
                    await self.files.list()
                elif command == "put":
                    self.require(args, 2, 3, "put local-file [remote-name]")
                    local = self.local_path(args[1])
                    await self.files.upload(local, args[2] if len(args) == 3 else local.name)
                elif command == "get":
                    self.require(args, 2, 3, "get remote-name [local-file]")
                    local = self.local_path(args[2] if len(args) == 3 else args[1])
                    await self.files.download(args[1], local)
                elif command in ("rm", "del", "delete"):
                    self.require(args, 2, 2, "rm remote-name")
                    await self.files.delete(args[1])
                elif command == "pwd":
                    print("Remote working directory: /lfs")
                elif command == "lpwd":
                    print(f"Local working directory: {self.local_cwd}")
                elif command == "lcd":
                    self.require(args, 2, 2, "lcd local-directory")
                    directory = self.local_path(args[1]).resolve()
                    if not directory.is_dir():
                        raise RuntimeError(f"not a directory: {directory}")
                    self.local_cwd = directory
                elif command == "lls":
                    self.require(args, 1, 2, "lls [local-directory]")
                    directory = self.local_path(args[1]) if len(args) == 2 else self.local_cwd
                    for entry in sorted(directory.iterdir()):
                        print(entry.name + ("/" if entry.is_dir() else ""))
                else:
                    print(f"unknown command: {command}")
            except EOFError:
                return
            except (BleakError, OSError, RuntimeError, ValueError) as error:
                print(f"error: {error}")

    @staticmethod
    def require(args, minimum: int, maximum: int, usage: str):
        if not minimum <= len(args) <= maximum:
            raise ValueError(f"usage: {usage}")

    @staticmethod
    def help():
        print("""Commands:
  ls                         list remote files
  put local [remote]         upload a file
  get remote [local]         download a file
  rm remote                  delete a remote file
  pwd / lpwd                 show remote/local directory
  lcd directory              change local directory
  lls [directory]            list local files
  quit                       disconnect and exit""")


async def find_device(address: Optional[str]):
    if address:
        return address
    print(f"scanning for {DEVICE_NAME}...")
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10)
    if not device:
        raise RuntimeError(f"BLE device {DEVICE_NAME!r} not found")
    return device


async def main(args):
    device = await find_device(args.address)
    async with BleakClient(device) as client:
        files = FileClient(client)
        await files.start()
        if args.command is None or args.command == "shell":
            await InteractiveShell(files).run()
        elif args.command == "list":
            await files.list()
        elif args.command == "upload":
            await files.upload(args.local, args.remote or args.local.name)
        elif args.command == "download":
            await files.download(args.remote, args.local or Path(args.remote))
        elif args.command == "delete":
            await files.delete(args.remote)


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--address", help="BLE address/UUID; otherwise scan by name")
    commands = parser.add_subparsers(dest="command")
    commands.add_parser("shell", help="open an interactive sftp-like session")
    commands.add_parser("list")

    upload = commands.add_parser("upload")
    upload.add_argument("local", type=Path)
    upload.add_argument("remote", nargs="?")

    download = commands.add_parser("download")
    download.add_argument("remote")
    download.add_argument("local", nargs="?", type=Path)

    delete = commands.add_parser("delete")
    delete.add_argument("remote")
    return parser.parse_args()


if __name__ == "__main__":
    asyncio.run(main(parse_args()))
