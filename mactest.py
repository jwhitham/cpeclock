
import hashlib
import hmac
import struct
import typing

MAX_STORE_SIZE = 0x100

class MAC:
    def __init__(self, secret_data: bytes) -> None:
        self.secret_data = secret_data
        self.lower_index = 0

    def save(self) -> bytes:
        return struct.pack("<Q", self.lower_index)

    def restore(self, state: bytes) -> None:
        (self.lower_index, ) = struct.unpack("<Q", state)

    def get_key_for_index(self, index: int) -> bytes:
        return self.secret_data + struct.pack("<Q", index)

    def encode_packet(self, payload: bytes) -> bytes:
        index = self.lower_index

        index_byte = struct.pack("<B", index & 0xff)

        mac_bytes = hmac.digest(
                key=self.get_key_for_index(index),
                msg=payload + index_byte,
                digest='sha256')[:7]

        self.lower_index = index + 1
        output = payload + index_byte + mac_bytes
        return output

    def decode_packet(self, packet: bytes) -> typing.Optional[bytes]:
        assert len(packet) > 8
        mac_bytes = packet[-7:]
        index_byte = packet[-8:-7]
        payload = packet[:-8]

        (partial_index, ) = struct.unpack("<B", index_byte)
        index = (self.lower_index & ~0xff) | partial_index
        if index < self.lower_index:
            index += 0x100

        if mac_bytes == hmac.digest(
                key=self.get_key_for_index(index),
                msg=payload + index_byte,
                digest='sha256')[:7]:
            # accepted - cancel used or skipped keys
            self.lower_index = index + 1
            return payload

        else:
            # not accepted
            return None

def test_normal():
    m1 = MAC(b"s")
    m2 = MAC(b"s")
    p = m1.encode_packet(b"1234")
    p2 = m2.decode_packet(p)
    assert p2 == b"1234"

def test_skip():
    m1 = MAC(b"s")
    m2 = MAC(b"s")
    for i in range(255):
        m1.encode_packet(b"z")
    p = m1.encode_packet(b"1234")
    p2 = m2.decode_packet(p)
    assert p2 == b"1234"

    for i in range(254):
        m1.encode_packet(b"z")
    p = m1.encode_packet(b"234")
    p2 = m2.decode_packet(p)
    assert p2 == b"234"

    p1 = m1.encode_packet(b"xx")
    for i in range(255):
        m1.encode_packet(b"z")
    p = m1.encode_packet(b"yy")
    p2 = m2.decode_packet(p)    # This is too far in the future and will be ignored
    assert p2 is None
    p2 = m2.decode_packet(p1)   # This is within range (index+1)
    assert p2 == b"xx"
    p2 = m2.decode_packet(p)    # Now p is decodable (index+256)
    assert p2 == b"yy"

def test_replay():
    m1 = MAC(b"s")
    m2 = MAC(b"s")
    replay = []
    for i in range(256):
        replay.append(m1.encode_packet(b"z"))
    for i in range(256):
        assert m2.decode_packet(replay[i]) == b"z"
        assert m1.decode_packet(replay[i]) is None
    for i in range(256):
        assert m2.decode_packet(replay[i]) is None

def test_replay():
    m1 = MAC(b"s")
    m2 = MAC(b"s")
    replay = []
    for i in range(256):
        replay.append(m1.encode_packet(b"z"))
    for i in range(256):
        assert m2.decode_packet(replay[i]) == b"z"
        assert m1.decode_packet(replay[i]) is None
    for i in range(256):
        assert m2.decode_packet(replay[i]) is None

def test_replay2():
    m1 = MAC(b"s")
    m2 = MAC(b"s")
    replay = []
    for i in range(256):
        replay.append(m1.encode_packet(b"z"))
    assert m2.decode_packet(replay[200]) == b"z"
    assert m1.decode_packet(replay[200]) is None
    for i in range(200):
        assert m2.decode_packet(replay[i]) is None
    for i in range(201, 256):
        assert m2.decode_packet(replay[i]) == b"z"

def test_wrong():
    m1 = MAC(b"s1")
    m2 = MAC(b"s2")
    p = m1.encode_packet(b"1234")
    p2 = m2.decode_packet(p)
    assert p2 is None
    m2 = MAC(b"s1")
    p2 = m2.decode_packet(b"2" + p[1:])
    assert p2 is None
    p2 = m2.decode_packet(p)
    assert p2 == b"1234"

def test_save_load():
    m1 = MAC(b"s")
    m2 = MAC(b"s")
    p = m1.encode_packet(b"1234")
    s2 = m2.save()
    p2 = m2.decode_packet(p)
    assert p2 == b"1234"
    p2 = m2.decode_packet(p)
    assert p2 is None
    m2.restore(s2)
    p2 = m2.decode_packet(p)
    assert p2 == b"1234"
    m1.restore(s2)
    m2.restore(s2)
    for i in range(300):
        p = m1.encode_packet(struct.pack("<I", i))
        p2 = m2.encode_packet(struct.pack("<I", i))
        assert p == p2
        assert m1.save() == m2.save()
        assert m1.lower_index == m2.lower_index

    for i in range(10):
        p = m1.encode_packet(struct.pack("<I", i))
        p2 = m2.encode_packet(b"X")
        assert p != p2
        assert m1.save() == m2.save()
        assert m1.lower_index == m2.lower_index

    p = m1.encode_packet(b"Y")
    p2 = m2.decode_packet(p)
    assert m1.lower_index == m2.lower_index
    assert p2 == b"Y"
    assert m1.save() == m2.save()

def main():
    with open("test_mac.bin", "wb") as fd:
        m = MAC(b"secret")
        for i in range(300):
            packet = "message {}".format(i).encode("ascii")
            data = m.encode_packet(packet)
            fd.write(struct.pack("<B", len(data)))
            fd.write(data)


if __name__ == "__main__":
    main()

