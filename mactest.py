
import hashlib
import struct
import typing

MAX_STORE_SIZE = 0x100

class MAC:
    def __init__(self, secret_data: bytes) -> None:
        self.secret_data = secret_data
        self.lower_index = 0
        self.upper_index = 1
        self.value_store = [b"" for i in range(MAX_STORE_SIZE)]
        self.value_store[self.lower_index % MAX_STORE_SIZE] = secret_data

    def save(self) -> bytes:
        index_byte = struct.pack("<B", self.lower_index & 0xff)
        state = self.value_store[self.lower_index % MAX_STORE_SIZE] + index_byte
        return state

    def restore(self, state: bytes) -> None:
        (partial_index, ) = struct.unpack("<B", state[-1:])
        self.lower_index = partial_index
        self.upper_index = partial_index + 1
        self.value_store = [b"" for i in range(MAX_STORE_SIZE)]
        self.value_store[self.lower_index % MAX_STORE_SIZE] = state[:-1]

    def get_store_size(self) -> int:
        return self.upper_index - self.lower_index

    def refill_one(self) -> None:
        if self.get_store_size() < MAX_STORE_SIZE:
            s = hashlib.sha256()
            s.update(self.value_store[(self.upper_index - 1) % MAX_STORE_SIZE])
            s.update(self.secret_data)
            self.value_store[self.upper_index % MAX_STORE_SIZE] = s.digest()
            self.upper_index += 1

    def refill_all(self) -> None:
        while self.get_store_size() < MAX_STORE_SIZE:
            self.refill_one()

    def cancel_index(self, index: int) -> None:
        assert index >= self.lower_index
        assert index < self.upper_index

        while index >= self.lower_index:
            if self.lower_index == (self.upper_index - 1):
                # store is about to become empty - ensure this does not happen
                self.refill_one()

            self.value_store[self.lower_index % MAX_STORE_SIZE] = b""
            self.lower_index += 1

    def encode_packet(self, payload: bytes) -> bytes:
        index = self.lower_index

        index_byte = struct.pack("<B", index & 0xff)

        s = hashlib.sha256()
        s.update(payload)
        s.update(index_byte)
        assert self.value_store[index % MAX_STORE_SIZE] != b""
        s.update(self.value_store[index % MAX_STORE_SIZE])
        mac_bytes = s.digest()[:7]

        self.cancel_index(index)
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

        while index >= self.upper_index:
            assert self.get_store_size() < MAX_STORE_SIZE
            self.refill_one()

        s = hashlib.sha256()
        s.update(payload)
        s.update(index_byte)
        assert self.value_store[index % MAX_STORE_SIZE] != b""
        s.update(self.value_store[index % MAX_STORE_SIZE])

        if mac_bytes == s.digest()[:7]:
            # accepted - cancel used or skipped keys
            self.cancel_index(index)
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
    m2.refill_all()
    assert m1.save() == m2.save()

def test_refill():
    m1 = MAC(b"s")
    m2 = MAC(b"s")
    p = m1.encode_packet(b"1234")
    p2 = m2.decode_packet(p)
    assert p2 == b"1234"
    m1.refill_all()
    p = m1.encode_packet(b"xxxx")
    p2 = m2.decode_packet(p)
    assert p2 == b"xxxx"

