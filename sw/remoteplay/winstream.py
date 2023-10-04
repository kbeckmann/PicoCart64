#!/bin/env python3

import socket
import struct
import time
import threading
import numpy as np

import dxcam
import win32api
import win32con

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

    # Send 1408 + 8 bytes at a time
    chunk_len = 128 * 11
    for offset in range(0, len(fb), chunk_len):
        m = struct.pack("<II", 0x42424242, offset) + fb[offset:offset+chunk_len]
        s.sendto(m, addr)
        # Throttle it a bit..
        time.sleep(0.0001)


def serve_tx(addr):
    # Received hello

    frame_n64 = bytearray(320 * 240 * 2)
    camera = dxcam.create()
    left, top = (1920 - 640) // 2, (1080 - 640) // 2
    right, bottom = left + 320, top + 240
    region = (left, top, right, bottom)

    frame = camera.start(region=region, target_fps=30)

    start = time.perf_counter()
    frames = 0

    while True:
        frame = camera.get_latest_frame()
        if frame is None:
            print("Fail")
            time.sleep(0.01)
            continue

        start2 = time.perf_counter()

        # Thanks, ChatGPT!

        # frame has the shape (320, 240, 3) and has dtype=uint8
        # extract the R, G, B channels
        r = frame[..., 0].astype('uint16')
        g = frame[..., 1].astype('uint16')
        b = frame[..., 2].astype('uint16')

        # rgb888 -> rgba5551
        rgba = np.left_shift(np.right_shift(r, 3), 11) \
            | np.left_shift(np.right_shift(g, 3), 6) \
            | np.left_shift(np.right_shift(b, 3), 1) \
            | 1

        # flatten the array and split the bytes
        frame_n64 = np.zeros((320 * 240,), dtype=np.uint16)
        frame_n64[:] = rgba.flatten()
        frame_n64_bytes = frame_n64.tobytes()

        frames += 1

        delta1 = time.perf_counter() - start
        # print(f"counter1: {frames/delta1}")

        delta2 = time.perf_counter() - start2
        # print(f"counter2: {delta2}")

        send_fb(addr, frame_n64_bytes)
        #time.sleep(0.001)

# // Pad buttons
def A_BUTTON(a):
    return    ((a) & 0x8000)
def B_BUTTON(a):
    return    ((a) & 0x4000)
def Z_BUTTON(a):
    return    ((a) & 0x2000)
def START_BUTTON(a):
    return ((a) & 0x1000)

# // D-Pad
def DU_BUTTON(a):
    return    ((a) & 0x0800)
def DD_BUTTON(a):
    return    ((a) & 0x0400)
def DL_BUTTON(a):
    return    ((a) & 0x0200)
def DR_BUTTON(a):
    return    ((a) & 0x0100)

# // Triggers
def TL_BUTTON(a):
    return    ((a) & 0x0020)
def TR_BUTTON(a):
    return    ((a) & 0x0010)

# // Yellow C buttons
def CU_BUTTON(a):
    return    ((a) & 0x0008)
def CD_BUTTON(a):
    return    ((a) & 0x0004)
def CL_BUTTON(a):
    return    ((a) & 0x0002)
def CR_BUTTON(a):
    return    ((a) & 0x0001)


def parse_buttons(btn):
    # BBPSX
    return {
        # Movement (DPAD)
        ord('W'): DU_BUTTON(btn),
        ord('S'): DD_BUTTON(btn),
        ord('A'): DL_BUTTON(btn),
        ord('D'): DR_BUTTON(btn),

        # Attack R
        win32con.VK_UP: TR_BUTTON(btn),
        # Attack L
        win32con.VK_DOWN: TL_BUTTON(btn),

        # Lock on / Interact (CPAD Down)
        ord('E'): CD_BUTTON(btn),
        # Use Quick Item (CPAD Up)
        ord('R'): CU_BUTTON(btn),

        # Cycle Quick Items: A
        win32con.VK_TAB: A_BUTTON(btn),
        # Dodge / Sprint (CPAD Right)
        win32con.VK_SPACE: CR_BUTTON(btn),
        # Walk (CPAD Left)
        win32con.VK_LCONTROL: CL_BUTTON(btn),

        # Pause (START)
        win32con.VK_ESCAPE: START_BUTTON(btn),

        # Select Menu Option
        win32con.VK_RETURN: B_BUTTON(btn),
    }

if __name__ == '__main__':
    old_joy_x = 0
    old_joy_y = 0
    old_btns = parse_buttons(0)

    for data, addr in udp_server():
        print(data, addr)
        if data == b'AAAA':
            thread = threading.Thread(target=serve_tx, args=(addr,), daemon=True)
            thread.start()
        elif data[:4] == b'CCCC':
            # Buttons
            # print("BUTTONS!!!")
            joy_x, joy_y = struct.unpack("bb", bytes(data[6:8]))
            btn = struct.unpack("<H", data[4:6])[0]

            if joy_x != old_joy_x or joy_y != old_joy_y:
                old_joy_x = joy_x
                old_joy_y = joy_y

                joy_x_adjusted = joy_x / 64.0
                joy_x_adjusted = max(-0.95, min(joy_x_adjusted, 0.95))
                joy_y_adjusted = -joy_y / 64.0
                joy_y_adjusted = max(-0.95, min(joy_y_adjusted, 0.95))
                print("joy_x", joy_x_adjusted)
                print("joy_y", joy_y_adjusted)
                
                if joy_x_adjusted < -0.5:
                    win32api.keybd_event(win32con.VK_RIGHT, 0, win32con.KEYEVENTF_KEYUP, 0)
                    win32api.keybd_event(win32con.VK_LEFT, 0, 0, 0)
                elif joy_x_adjusted < 0.5:
                    win32api.keybd_event(win32con.VK_RIGHT, 0, win32con.KEYEVENTF_KEYUP, 0)
                    win32api.keybd_event(win32con.VK_LEFT, 0, win32con.KEYEVENTF_KEYUP, 0)
                else:
                    win32api.keybd_event(win32con.VK_RIGHT, 0, 0, 0)
                    win32api.keybd_event(win32con.VK_LEFT, 0, win32con.KEYEVENTF_KEYUP, 0)

            new_btns = parse_buttons(btn)

            for _, k in enumerate(new_btns):
                v = new_btns[k]
                # print(f"key {k}={v}")
                if v != old_btns[k]:
                    win32api.keybd_event(k, 0, win32con.KEYEVENTF_KEYUP if v == 0 else 0, 0)
                    # print(f"key {k}={v}")

            old_btns = new_btns
