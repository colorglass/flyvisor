import argparse
import logging
import serial
import threading
import socket
from ctypes import *
from ctypes.util import find_library

key = [0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd,
                       0xef, 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54,
                       0x32, 0x10]

iv = [0xab, 0x23, 0x9e, 0x45, 0x89, 0xab,
                      0xcd, 0xef, 0xfe, 0xdc, 0xba, 0x98,
                      0x76, 0x4a, 0x1f, 0x10]

key = bytes(key)
iv = bytes(iv)

gmssl = cdll.LoadLibrary(find_library("gmssl"))
sm4_decrypt_key = create_string_buffer(128)
sm4_encrypt_key = create_string_buffer(128)
gmssl.sm4_set_decrypt_key(byref(sm4_decrypt_key), key)
gmssl.sm4_set_encrypt_key(byref(sm4_encrypt_key), key)

parser = argparse.ArgumentParser(description='MAVLink proxy for encrypted communication')
parser.add_argument('serial', help='Serial port to use')
parser.add_argument('-b', '--baudrate', help='Serial baudrate', default=57600)
parser.add_argument('-d', '--debug', help='Enable debug output', action='store_true', default=False)
args = parser.parse_args()

logger_main = logging.getLogger('main')
if args.debug:
    logging.basicConfig(level=logging.DEBUG)
else:
    logging.basicConfig(level=logging.INFO)

try:
    serial_in = serial.Serial(args.serial, args.baudrate)
except serial.SerialException as e:
    logger_main.error('Could not open serial port: %s', e)
    exit(1)

logger_main.info('Opened serial port %s with %s', args.serial, args.baudrate)

self_addr = ('169.254.75.91', 14551)
target_addr = ('169.254.23.45', 14550)
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(self_addr)

def serial_to_udp():
    logger = logging.getLogger("recv | decrypt")
    outbuf = create_string_buffer(1024)
    outlen = c_size_t()
    while True:
        mag = serial_in.read(1)
        if mag == b'\xe7':
            fun = serial_in.read(1)
            if fun == b'\04':
                len = serial_in.read(2)
                len = int.from_bytes(len, byteorder='little')
                chiper_data = serial_in.read(len)
                logger.debug('chipertext: %s', chiper_data.hex())
                gmssl.sm4_cbc_padding_decrypt(byref(sm4_decrypt_key), iv, chiper_data, len, outbuf, byref(outlen))
                plain_data = outbuf[:outlen.value]
                logger.debug('Received: %s', plain_data.hex())
                sock.sendto(plain_data, target_addr)

def udp_to_serial():
    logger = logging.getLogger('send | encrypt')
    outbuf = create_string_buffer(1024)
    outlen = c_size_t()
    while True:
        plain_data = sock.recv(1024)
        logger.debug('plaintext: %s', plain_data.hex())
        gmssl.sm4_cbc_padding_encrypt(byref(sm4_encrypt_key), iv, plain_data, len(plain_data), outbuf, byref(outlen))
        chiper_data = outbuf[:outlen.value]
        logger.debug('chipertext: %s', chiper_data.hex())
        serial_in.write(b'\xe7\04' + len(chiper_data).to_bytes(2, byteorder='little') + chiper_data)
        

threading.Thread(target=serial_to_udp).start()
threading.Thread(target=udp_to_serial).start()
