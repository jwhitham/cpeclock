
import hashlib
import hmac
import struct
import typing
import os

SECRET_SIZE = 56
STATE_FILE = os.path.join(os.environ.get("APPDATA",
                    os.environ.get("HOME", ".")), ".hmac433.dat")


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

        index_byte = struct.pack("<B", index & 0xff)

        mac_bytes = hmac.digest(
                key=self.get_key_for_index(index),
                msg=payload + index_byte,
                digest='sha256')[:7]

        self.counter = index + 1
        output = payload + index_byte + mac_bytes
        return output

    def decode_packet(self, packet: bytes) -> typing.Optional[bytes]:
        assert len(packet) > 8
        mac_bytes = packet[-7:]
        index_byte = packet[-8:-7]
        payload = packet[:-8]

        (partial_index, ) = struct.unpack("<B", index_byte)
        index = (self.counter & ~0xff) | partial_index
        if index < self.counter:
            index += 0x100

        if mac_bytes == hmac.digest(
                key=self.get_key_for_index(index),
                msg=payload + index_byte,
                digest='sha256')[:7]:
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
