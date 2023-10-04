#!/bin/env python3

import logging
import socket
import struct
import time

log = logging.getLogger('udp_server')
UDP_PORT = 1111

# fb = open("out_565le.rgb", "rb").read()
fb = open("out_rgb5551.rgb", "rb").read()

def send_fb(addr):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, 0)

    for offset in range(0, len(fb), 1024):
        m = struct.pack("<II", 0x42424242, offset) + fb[offset:offset+1024]
        s.sendto(m, addr)
        # time.sleep(1)


def udp_server(host='0.0.0.0', port=UDP_PORT):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    log.info("Listening on udp %s:%s" % (host, port))
    s.bind((host, port))
    while True:
        (data, addr) = s.recvfrom(128*1024)
        yield data, addr

for data, addr in udp_server():
    print(data, addr)
    if data == b'AAAA':
        # Received hello
        send_fb(addr)
