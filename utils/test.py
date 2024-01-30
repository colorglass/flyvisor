from ctypes import *
from ctypes.util import find_library
import sys

gmssl = cdll.LoadLibrary(find_library("gmssl"))

key = [0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd,
                       0xef, 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54,
                       0x32, 0x10]

iv = [0xab, 0x23, 0x9e, 0x45, 0x89, 0xab,
                      0xcd, 0xef, 0xfe, 0xdc, 0xba, 0x98,
                      0x76, 0x4a, 0x1f, 0x10]

# converte key into bytes type
key = bytes(key)
iv = bytes(iv)

# plain_data = fd0900009701010000000000000002035103033ca5
chiper_data = bytes.fromhex('08010c22a19947be4af5eddce55eb397')
print(len(chiper_data))
sm4_key = create_string_buffer(128)
gmssl.sm4_set_decrypt_key(byref(sm4_key), key)
outbuf = create_string_buffer(1024)
outlen = c_size_t()
gmssl.sm4_cbc_padding_decrypt(byref(sm4_key), iv, chiper_data, len(chiper_data), outbuf, byref(outlen))
plain = outbuf[:outlen.value]
print(outlen.value)
print(plain)
