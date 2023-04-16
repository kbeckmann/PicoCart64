#!/bin/env python3

import time
import numpy as np


def capture_and_store_frame():
    import dxcam
    left, top = 0, 0
    right, bottom = left + 320, top + 240
    region = (left, top, right, bottom)

    camera = dxcam.create()
    frame = camera.grab(region=region)

    with open("frame.rgb888", "wb") as f:
        f.write(frame.tobytes())
        f.close()

def load_frame():
    # (Height,  Width, 3[RGB])
    return np.fromfile("frame.rgb888", dtype=np.uint8).reshape(240, 320, 3)

def convert_rgb888_to_rgb5551(frame):
    start = time.perf_counter()

    # frame has the shape (240, 320, 3) and has dtype=uint8
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

    delta = time.perf_counter() - start
    print(f"Took: {delta * 1000} ms")

    return frame_n64_bytes

def save(frame_rgb5551):
    with open("frame.rgb5551", "wb") as f:
        f.write(frame_rgb5551)
        f.close()

def compare(frame_rgb5551):
    with open("frame.rgb5551", "rb") as f:
        facit = f.read()
        f.close()

    if facit == frame_rgb5551:
        print("OK")
    else:
        print("Files differ, wrote frame_bad.rgb5551")
        with open("frame_bad.rgb5551", "wb") as f:
            f.write(frame_rgb5551)
            f.close()


if __name__ == '__main__':

    # Uncomment to use dxcam to capture a real frame
    # capture_and_store_frame()

    frame_rgb888 = load_frame()

    # Uncomment to look at reference rgb888 image
    # from PIL import Image
    # Image.fromarray(frame_rgb888).show()

    frame_rgb5551 = convert_rgb888_to_rgb5551(frame_rgb888)

    # Uncomment to save a new reference rgb5551 image
    # save(frame_rgb5551)

    compare(frame_rgb5551)


