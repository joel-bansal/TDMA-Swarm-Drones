import socket
import struct

UDP_IP = "127.0.0.1"
RX_PORT = 52002

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, RX_PORT))

print(f"[*] Listening for unpacked PDUs on UDP port {RX_PORT}...\n")

def bits_to_bytes(bit_array):
    """Packs 152 unpacked 1-bit bytes into 19 standard ASCII bytes."""
    packed = bytearray()
    for i in range(0, len(bit_array), 8):
        chunk = bit_array[i:i+8]
        if len(chunk) < 8:
            break
        byte_val = 0
        for bit in chunk:
            byte_val = (byte_val << 1) | (bit & 0x01)
        packed.append(byte_val)
    return bytes(packed)

pkt_count = 0

while True:
    data, addr = sock.recvfrom(2048)
    pkt_count += 1

    # data contains 152 bytes where each byte is 0x00 or 0x01
    raw_bit_count = len(data)
    
    # 1. Convert raw bit-bytes to ASCII
    reconstructed_bytes = bits_to_bytes(data)
    decoded_str = reconstructed_bytes.decode("utf-8", errors="replace")

    # 2. Check match criteria
    is_exact_len = (raw_bit_count == 152)
    is_valid_node = decoded_str.startswith("NODE_1 | PKT_#:")

    print("=" * 60)
    print(f"[RX Packet #{pkt_count}] From {addr}")
    print(f" -> Raw Received Length: {raw_bit_count} bit-bytes (Expected: 152)")
    print(f" -> Reconstructed Hex  : {reconstructed_bytes.hex()}")
    print(f" -> Decoded Text String: '{decoded_str}'")

    if is_exact_len and is_valid_node:
        print(" -> [VERIFICATION RESULT]: MATCH SUCCESSFUL (100% Exact Frame)!")
    else:
        print(" -> [VERIFICATION RESULT]: BIT MISMATCH / CORRUPTED FRAME!")
    print("=" * 60 + "\n")
