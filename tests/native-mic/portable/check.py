"""Exercise the shared sender and SDL capture with a dummy input and UDP loopback."""
import ctypes as c
import os
from pathlib import Path
import socket
import struct
import time
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

os.environ["SDL_AUDIODRIVER"] = "dummy"
base = Path(__file__).resolve().parent
mic = c.CDLL(str(base / "libportable-mic-test.dylib"))
mic.testConfigure.argtypes = [c.c_char_p, c.c_ushort, c.c_int, c.c_int]
mic.startNativeMic.argtypes = [c.c_char_p, c.c_ushort, c.c_char_p, c.c_char_p]
mic.LiCreateNativeMicSender.restype = c.c_void_p
opus = c.CDLL(str(base.parents[2] / "libs/mac/lib/libopus.0.dylib"))
opus.opus_decoder_create.argtypes = [c.c_int, c.c_int, c.POINTER(c.c_int)]
opus.opus_decoder_create.restype = c.c_void_p
opus.opus_decode.argtypes = [c.c_void_p, c.c_char_p, c.c_int, c.POINTER(c.c_int16), c.c_int, c.c_int]
opus.opus_decoder_destroy.argtypes = [c.c_void_p]
error = c.c_int()
decoder = opus.opus_decoder_create(48000, 1, c.byref(error))
assert decoder and error.value == 0

with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as receiver:
    receiver.bind(("127.0.0.1", 0))
    port = receiver.getsockname()[1]
    receiver.settimeout(3)
    try:
        for params in [
            (b"8.8.8.8", port, 1, 1),
            (b"127.0.0.1", 0, 1, 1),
            (b"127.0.0.1", port, 0, 1),
            (b"127.0.0.1", port, 1, 0),
        ]:
            mic.testConfigure(*params)
            assert not mic.LiCreateNativeMicSender()

        mic.testConfigure(b"127.0.0.1", port, 1, 1)
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as occupied:
            occupied.bind(("127.0.0.1", 50002))
            assert mic.startNativeMic(None, 0, None, None) == 3

        assert mic.startNativeMic(None, 0, None, None) == 0
        frames = pings = 0
        start = time.monotonic()
        while frames < 300:
            packet, source = receiver.recvfrom(128)
            assert source[1] == 50002
            if packet == b"PING":
                pings += 1
                continue
            assert len(packet) == 44
            assert struct.unpack("!BBHII", packet[:12]) == (128, 97, frames, frames * 5, 1)
            iv = struct.pack("!I", 0x12345678 + frames) + bytes(12)
            decryptor = Cipher(algorithms.AES(b"0123456789abcdef"), modes.CBC(iv)).decryptor()
            padded = decryptor.update(packet[12:]) + decryptor.finalize()
            assert padded[20:] == bytes([12]) * 12
            pcm = (c.c_int16 * 240)()
            assert opus.opus_decode(decoder, padded[:20], 20, pcm, 240, 0) == 240
            frames += 1
        assert pings >= 3 and 1 < time.monotonic() - start < 3
        assert mic.nativeMicStatus() == 1
        mic.stopNativeMic()
        assert mic.nativeMicStatus() == 0
        receiver.settimeout(.1)
        try:
            while True:
                receiver.recvfrom(128)
        except socket.timeout:
            pass
        for _ in range(10):
            assert mic.startNativeMic(None, 0, None, None) == 0
            mic.stopNativeMic()
            assert mic.nativeMicStatus() == 0
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as free:
            free.bind(("127.0.0.1", 50002))
        print(f"PASS: {frames} independently decrypted/decoded SDL dummy frames, {pings} pings, four session gates, occupied port and ten restart cycles")
    finally:
        mic.stopNativeMic()
        opus.opus_decoder_destroy(decoder)
