#!/usr/bin/env python3
import argparse
import asyncio
import contextlib
import os
import sys

from bleak import BleakClient, BleakScanner

NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
NUS_TX_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"


@contextlib.contextmanager
def stdin_cbreak():
    if not sys.stdin.isatty():
        yield
        return

    try:
        import termios
    except ImportError:
        yield
        return

    fd = sys.stdin.fileno()
    old_attrs = termios.tcgetattr(fd)
    new_attrs = old_attrs[:]
    new_attrs[3] &= ~(termios.ICANON | termios.ECHO)
    new_attrs[6][termios.VMIN] = 1
    new_attrs[6][termios.VTIME] = 0
    # Disable VINTR (Ctrl+C) signal generation so we can handle it ourselves
    new_attrs[6][termios.VINTR] = 0

    try:
        termios.tcsetattr(fd, termios.TCSADRAIN, new_attrs)
        yield
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_attrs)


async def find_device(name: str, timeout: float):
    print(f"Scanning for {name!r}...", file=sys.stderr)

    def match(device, adv):
        names = {device.name, adv.local_name}
        return name in names or NUS_SERVICE_UUID.lower() in adv.service_uuids

    device = await BleakScanner.find_device_by_filter(match, timeout=timeout)
    if device is None:
        raise RuntimeError(f"Could not find BLE device named {name!r}")

    print(f"Found device: {device.name} ({device.address})", file=sys.stderr)
    return device


async def stdin_bytes(queue: asyncio.Queue):
    loop = asyncio.get_running_loop()
    fd = sys.stdin.fileno()

    while True:
        data = await loop.run_in_executor(None, os.read, fd, 16)
        if data == b"":
            await queue.put(None)
            return
        # Exit on Ctrl+D (0x04), send everything else to device
        if b"\x04" in data:
            await queue.put(None)
            return
        await queue.put(data)


async def run(args):
    device = args.address or await find_device(args.name, args.scan_timeout)

    print(f"Connecting to {device}...", file=sys.stderr)
    async with BleakClient(device) as client:
        if not client.is_connected:
            raise RuntimeError("BLE connection failed")

        print("Connected. Type shell commands; Ctrl-D exits.", file=sys.stderr)

        def on_rx(_, data: bytearray):
            text = data.decode(errors="replace")
            print(text, end="", flush=True)

        await client.start_notify(NUS_TX_UUID, on_rx)

        # Send a newline to trigger shell prompt
        await client.write_gatt_char(NUS_RX_UUID, b"\r", response=False)

        queue = asyncio.Queue()
        stdin_task = asyncio.create_task(stdin_bytes(queue))
        writer_task = asyncio.create_task(
            write_stdin_to_nus(client, queue, args.with_response)
        )

        try:
            await writer_task
        finally:
            stdin_task.cancel()
            with contextlib.suppress(asyncio.CancelledError):
                await stdin_task
            await client.stop_notify(NUS_TX_UUID)


async def write_stdin_to_nus(
    client: BleakClient,
    queue: asyncio.Queue,
    with_response: bool,
):
    while True:
        data = await queue.get()
        if data is None:
            return

        data = data.replace(b"\n", b"\r")
        await client.write_gatt_char(NUS_RX_UUID, data, response=with_response)


def parse_args():
    parser = argparse.ArgumentParser(
        description="BLE Nordic UART Service shell terminal"
    )
    parser.add_argument("-n", "--name", default="C406Pro_Hack", help="BLE device name")
    parser.add_argument("-a", "--address", help="BLE address/UUID to connect directly")
    parser.add_argument(
        "--scan-timeout", type=float, default=10.0, help="scan timeout in seconds"
    )
    parser.add_argument(
        "--with-response",
        action="store_true",
        help="write RX characteristic with response instead of write without response",
    )
    return parser.parse_args()


def main():
    try:
        with stdin_cbreak():
            asyncio.run(run(parse_args()))
    except KeyboardInterrupt:
        pass
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
