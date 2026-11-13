import socket
import struct
import time

UDP_IP = "127.0.0.1"
UDP_PORT = 52001

ACCESS_CODE = bytes.fromhex("e546ba38")
NODE_ID = 1

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
packet_counter = 0

while True:
    packet_counter += 1

    # Include custom application fields inside the payload
    payload = f"NODE_{NODE_ID} | PKT_#:{packet_counter:04d}".encode("utf-8")
    payload_len = len(payload)

    # GNU Radio expects 16-bit payload_len repeated twice immediately after Access Code
    # bit_len = payload_len * 8  # 19 * 8 = 152
    # gr_header = struct.pack(">HH", bit_len, bit_len)
    gr_header = struct.pack(">HH", payload_len, payload_len)

    # Complete Frame
    frame = ACCESS_CODE + gr_header + payload

    sock.sendto(frame, (UDP_IP, UDP_PORT))

    print(
        f"[TX] seq={packet_counter} "
        f"payload_len={payload_len} "
        f"frame_len={len(frame)} "
        f"hex={frame.hex()}",
        flush=True
    )

    time.sleep(0.1)
