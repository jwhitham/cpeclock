
import hashlib
import hmac
import struct
import typing
import os

SECRET_SIZE = 56
STATE_FILE = os.path.join(os.environ.get("APPDATA",
                    os.environ.get("HOME", ".")), ".hmac433.dat")
PACKET_PAYLOAD_SIZE = 6
PACKET_HMAC_SIZE = 6
PACKET_SIZE = (PACKET_HMAC_SIZE + 1 + PACKET_PAYLOAD_SIZE)


class HMAC433:
    def __init__(self, secret_data = b"\x01\x02\x03\x04") -> None:
        self.secret_data = secret_data
        self.counter = 0

    def save(self) -> bytes:
        assert len(self.secret_data) == SECRET_SIZE
        return (struct.pack("<Q", self.counter) + self.secret_data)

    def restore(self, state: bytes) -> None:
        (self.counter, ) = struct.unpack("<Q", state[:8])
        self.secret_data = state[8:]
        assert len(self.secret_data) == SECRET_SIZE

    def get_key_for_index(self, index: int) -> bytes:
        return self.secret_data + struct.pack("<Q", index)

    def encode_packet(self, payload: bytes) -> bytes:
        index = self.counter

        while len(payload) < PACKET_PAYLOAD_SIZE:
            payload += b"\x00"
        payload = payload[:PACKET_PAYLOAD_SIZE]

        index_byte = struct.pack("<B", index & 0xff)

        mac_bytes = hmac.digest(
                key=self.get_key_for_index(index),
                msg=payload,
                digest='sha256')[:PACKET_HMAC_SIZE]

        self.counter = index + 1
        output = payload + index_byte + mac_bytes
        return output

    def decode_packet(self, packet: bytes) -> typing.Optional[bytes]:
        assert len(packet) == PACKET_SIZE
        mac_bytes = packet[PACKET_PAYLOAD_SIZE + 1:]
        index_byte = packet[PACKET_PAYLOAD_SIZE:PACKET_PAYLOAD_SIZE + 1]
        payload = packet[:PACKET_PAYLOAD_SIZE]

        (partial_index, ) = struct.unpack("<B", index_byte)
        index = (self.counter & ~0xff) | partial_index
        if index < self.counter:
            index += 0x100

        if mac_bytes == hmac.digest(
                key=self.get_key_for_index(index),
                msg=payload,
                digest='sha256')[:PACKET_HMAC_SIZE]:
            # accepted - cancel used or skipped keys
            self.counter = index + 1
            return payload

        else:
            # not accepted
            return None

    def __enter__(self) -> "HMAC433":
        if os.path.isfile(STATE_FILE):
            with open(STATE_FILE, "rb") as fd:
                self.restore(fd.read())
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        with open(STATE_FILE, "wb") as fd:
            fd.write(self.save())
