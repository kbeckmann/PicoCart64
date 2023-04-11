"""Example of running client.

We are assuming that we have already linked a PSN profile to our Remote Play device.
"""

import asyncio
import threading
import atexit
import time
import struct
import socket

from pyremoteplay import RPDevice
from pyremoteplay.receiver import QueueReceiver

UDP_PORT = 1111

def udp_server(host='0.0.0.0', port=UDP_PORT):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    print("Listening on udp %s:%s" % (host, port))
    s.bind((host, port))
    while True:
        (data, addr) = s.recvfrom(128*1024)
        yield data, addr

def send_fb(addr, fb):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, 0)

    for offset in range(0, len(fb), 1024):
        m = struct.pack("<II", 0x42424242, offset) + fb[offset:offset+1024]
        s.sendto(m, addr)


def stop(device, thread):
    loop = device.session.loop
    device.disconnect()
    loop.stop()
    thread.join(3)
    print("stopped")


def worker(device):
    loop = asyncio.new_event_loop()
    task = loop.create_task(device.connect())
    loop.run_until_complete(task)
    loop.run_forever()


def start(ip_address):
    """Return device. Start Remote Play session."""
    device = RPDevice(ip_address)
    if not device.get_status():  # Device needs a valid status to get users
        print("No Status")
        return None
    users = device.get_users()
    if not users:
        print("No users registered")
        return None
    user = users[0]  # Gets first user name
    receiver = QueueReceiver()
    device.create_session(user, receiver=receiver, resolution="360p", fps="low")
    thread = threading.Thread(target=worker, args=(device,), daemon=True)
    thread.start()
    atexit.register(
        lambda: stop(device, thread)
    )  # Make sure we stop the thread on exit.

    # Wait for session to be ready
    device.wait_for_session()
    return device


if __name__ == '__main__':
    ip_address = '192.168.5.27' # ip address of Remote Play device
    device = start(ip_address)

    # Just wait forever
    loop = asyncio.new_event_loop()


    frame_n64 = []

    for data, addr in udp_server():
        print(data, addr)
        if data == b'AAAA':
            # Received hello
            while True:
                if len(device.session.receiver.video_frames) > 0:
                    print("Got a frame")
                    last_frame = device.session.receiver.video_frames[-1]

                    # Convert 640x360 to 320x180
                    frame = last_frame.reformat(320, 180, format="rgb24")
                    f = open("out_rgb24.rgb", "wb")
                    f.write(bytes(last_frame.planes[0]))
                    f.close()

                    # Convert to 565le (needed in a separate step!)
                    frame = frame.reformat(320, 180, format="rgb565le")
                    f = open("out_565le.rgb", "wb")
                    f.write(bytes(frame.planes[0]))
                    f.close()

                    # Manually convert to rgb5551
                    frame_565 = bytes(frame.planes[0])

                    # allocate
                    if len(frame_n64) == 0:
                        frame_n64 = bytearray(len(frame_565))

                    pixels = struct.unpack(f"<{len(frame_565) // 2}H", frame_565)
                    for i, x in enumerate(pixels):
                        # Remove LSB from the green channel
                        # r = (x & 0b11111_000000_00000) >> 11
                        # g = (x & 0b00000_111111_00000) >> 6
                        # b = (x & 0b00000_000000_11111)
                        # rgb = (r << 11) | (g << 6) | (b << 1) | 1

                        rgb = (x & 0b1111111111000000) | ((x << 1) & 0b111110) | 1

                        frame_n64[2*i] = rgb & 0xff
                        frame_n64[2*i+1] = (rgb >> 8) & 0xff

                    f = open("out_rgb5551.rgb", "wb")
                    f.write(frame_n64)
                    f.close()

                    send_fb(addr, frame_n64)

                time.sleep(0.001)

    loop.run_forever()




# Usage:
#
# Starting session:
# >> ip_address = '192.168.86.2' # ip address of Remote Play device
# >> device = start(ip_address)
#
# Retrieving latest video frames:
# >> device.session.receiver.video_frames
#
# Tap Controller Button:
# >> device.controller.button("cross", "tap")
#
# Start Controller Stick Worker
# >> device.controller.start()
#
# Emulate moving Left Stick all the way right:
# >> device.controller.stick("left", axis="x", value=1.0)
#
# Release Left stick:
# >> device.controller.stick("left", axis="x", value=0)
#
# Move Left stick diagonally left and down halfway
# >> device.controller.stick("left", point=(-0.5, 0.5))
#
# Standby; Only available when session is connected:
# >> device.session.standby()
#
# Wakeup/turn on using first user:
# >> device.wakeup(device.get_users[0])