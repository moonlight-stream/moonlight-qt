"""Synthetic-only regression tests of the actual macOS sender; no mic capture."""
from pathlib import Path
import ctypes, socket, struct, time, math, os, ctypes.util
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
root=Path(__file__).resolve().parent
mic=ctypes.CDLL(str(root/'libnative-mic-test.dylib'))
mic.startNativeTone.argtypes=[ctypes.c_char_p,ctypes.c_ushort,ctypes.c_char_p,ctypes.c_char_p]
mic.startNativeTone.restype=ctypes.c_int
mic.nativeMicStatus.restype=ctypes.c_int
key=b'0123456789abcdef';iv=bytes.fromhex('12345678')+bytes(12)
checks=0
def check(value):
 global checks
 assert value
 checks+=1
try:
 check(mic.startNativeTone(b'8.8.8.8',48002,key,iv)==3)
 check(mic.startNativeTone(b'127.0.0.1',0,key,iv)==3)
 check(mic.startNativeTone(b'127.0.0.1',48002,None,iv)==3)
 check(mic.testInvalidCaptureBuffer()==-1)
 check(mic.nativeMicStatus()==0)
 occupied=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
 occupied.bind(('127.0.0.1',50002))
 check(mic.startNativeTone(b'127.0.0.1',48002,key,iv)==3)
 occupied.close()
 receiver=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);receiver.bind(('127.0.0.1',0));receiver.settimeout(5)
 opus=ctypes.CDLL(os.environ.get('OPUS_LIBRARY') or ctypes.util.find_library('opus') or '/opt/homebrew/opt/opus/lib/libopus.0.dylib')
 opus.opus_decoder_create.argtypes=[ctypes.c_int,ctypes.c_int,ctypes.POINTER(ctypes.c_int)];opus.opus_decoder_create.restype=ctypes.c_void_p
 opus.opus_decode.argtypes=[ctypes.c_void_p,ctypes.c_char_p,ctypes.c_int,ctypes.POINTER(ctypes.c_int16),ctypes.c_int,ctypes.c_int]
 opus.opus_decoder_destroy.argtypes=[ctypes.c_void_p]
 error=ctypes.c_int();decoder=opus.opus_decoder_create(48000,1,ctypes.byref(error));check(bool(decoder) and error.value==0)
 check(mic.startNativeTone(b'127.0.0.1',receiver.getsockname()[1],key,iv)==0)
 pings=0;packets=0;peak=0;samples=[];started=None;last=None
 while packets<300:
  data,source=receiver.recvfrom(256)
  check(source[1]==50002)
  if data==b'PING':pings+=1;continue
  now=time.monotonic()
  if started is None:started=now
  version,kind,seq,stamp,ssrc=struct.unpack('!BBHII',data[:12])
  check((len(data),version,kind,ssrc)==(44,128,97,1))
  check((seq,stamp)==(packets,packets*5))
  prefix=struct.pack('!I',(0x12345678+seq)&0xffffffff)+bytes(12)
  decrypt=Cipher(algorithms.AES(key),modes.CBC(prefix)).decryptor()
  padded=decrypt.update(data[12:])+decrypt.finalize();check(padded[20:]==bytes([12])*12)
  pcm=(ctypes.c_int16*240)();check(opus.opus_decode(decoder,padded[:20],20,pcm,240,0)==240)
  samples.extend(pcm);peak=max(peak,max(map(abs,pcm)));packets+=1;last=now
 check(peak>1000)
 check(pings>=3)
 check(1.0<last-started<3.0)
 amplitude=abs(sum(v*complex(math.cos(2*math.pi*i/48),math.sin(2*math.pi*i/48)) for i,v in enumerate(samples)))*2/len(samples)
 check(amplitude>1000)
 mic.stopNativeMic();check(mic.nativeMicStatus()==0)
 receiver.settimeout(.05)
 try:
  while True:receiver.recvfrom(256)
 except socket.timeout:pass
 receiver.settimeout(.15)
 try:receiver.recvfrom(256);raise AssertionError('Packet after stop')
 except socket.timeout:check(True)
 opus.opus_decoder_destroy(decoder);receiver.close()
 for _ in range(10):
  check(mic.startNativeTone(b'127.0.0.1',48002,key,iv)==0)
  mic.stopNativeMic();check(mic.nativeMicStatus()==0)
 free=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);free.bind(('127.0.0.1',50002));free.close();check(True)
 print(f'PASS: {checks} sender assertions; {packets} encrypted packets, {pings} pings, mono Opus peak {peak}, 1kHz amplitude {amplitude:.1f}; no capture')
finally:mic.stopNativeMic()
