
import struct
import typing
import pytest
import sys

from hmac433 import HMAC433 as MAC
from hmac433 import SECRET_SIZE, PACKET_PAYLOAD_SIZE, PACKET_SIZE

TEST_PAYLOAD = "".join([chr(x + 0x41) for x in range(PACKET_PAYLOAD_SIZE)]).encode("ascii")

def test_normal():
    m1 = MAC(b"s")
    m2 = MAC(b"s")
    p = m1.encode_packet(TEST_PAYLOAD)
    p2 = m2.decode_packet(p)
    assert p2 == TEST_PAYLOAD

def test_skip():
    m1 = MAC(b"s")
    m2 = MAC(b"s")
    for i in range(255):
        m1.encode_packet(b"z")
    p = m1.encode_packet(TEST_PAYLOAD)
    p2 = m2.decode_packet(p)
    assert p2 == TEST_PAYLOAD

    for i in range(254):
        m1.encode_packet(b"z")
    p = m1.encode_packet(b"234")
    p2 = m2.decode_packet(p)
    assert p2.startswith(b"234")

    p1 = m1.encode_packet(b"xx")
    for i in range(255):
        m1.encode_packet(b"z")
    p = m1.encode_packet(b"yy")
    p2 = m2.decode_packet(p)    # This is too far in the future and will be ignored
    assert p2 is None
    p2 = m2.decode_packet(p1)   # This is within range (index+1)
    assert p2.startswith(b"xx")
    p2 = m2.decode_packet(p)    # Now p is decodable (index+256)
    assert p2.startswith(b"yy")

def test_replay():
    m1 = MAC(b"s")
    m2 = MAC(b"s")
    replay = []
    for i in range(256):
        replay.append(m1.encode_packet(TEST_PAYLOAD))
    for i in range(256):
        assert m2.decode_packet(replay[i]) == TEST_PAYLOAD
        assert m1.decode_packet(replay[i]) is None
    for i in range(256):
        assert m2.decode_packet(replay[i]) is None

def test_replay():
    m1 = MAC(b"s")
    m2 = MAC(b"s")
    replay = []
    for i in range(256):
        replay.append(m1.encode_packet(TEST_PAYLOAD))
    for i in range(256):
        assert m2.decode_packet(replay[i]) == TEST_PAYLOAD
        assert m1.decode_packet(replay[i]) is None
    for i in range(256):
        assert m2.decode_packet(replay[i]) is None

def test_replay2():
    m1 = MAC(b"s")
    m2 = MAC(b"s")
    replay = []
    for i in range(256):
        replay.append(m1.encode_packet(b"z"))
    assert m2.decode_packet(replay[200]).startswith(b"z")
    assert m1.decode_packet(replay[200]) is None
    for i in range(200):
        assert m2.decode_packet(replay[i]) is None
    for i in range(201, 256):
        assert m2.decode_packet(replay[i]).startswith(b"z")

def test_wrong():
    m1 = MAC(b"s1")
    m2 = MAC(b"s2")
    p = m1.encode_packet(TEST_PAYLOAD)
    p2 = m2.decode_packet(p)
    assert p2 is None
    m2 = MAC(b"s1")
    p2 = m2.decode_packet(b"2" + p[1:])
    assert p2 is None
    p2 = m2.decode_packet(p)
    assert p2 == TEST_PAYLOAD

def test_save_load():
    m1 = MAC(b"s" * SECRET_SIZE)
    m2 = MAC(b"s" * SECRET_SIZE)
    p = m1.encode_packet(TEST_PAYLOAD)
    s2 = m2.save()
    p2 = m2.decode_packet(p)
    assert p2 == TEST_PAYLOAD
    p2 = m2.decode_packet(p)
    assert p2 is None
    m2.restore(s2)
    p2 = m2.decode_packet(p)
    assert p2 == TEST_PAYLOAD
    m1.restore(s2)
    m2.restore(s2)
    for i in range(300):
        p = m1.encode_packet(struct.pack("<I", i))
        p2 = m2.encode_packet(struct.pack("<I", i))
        assert p == p2
        assert m1.save() == m2.save()
        assert m1.counter == m2.counter

    for i in range(10):
        p = m1.encode_packet(struct.pack("<I", i))
        p2 = m2.encode_packet(b"X")
        assert p != p2
        assert m1.save() == m2.save()
        assert m1.counter == m2.counter

    p = m1.encode_packet(b"Y")
    p2 = m2.decode_packet(p)
    assert m1.counter == m2.counter
    assert p2.startswith(b"Y")
    assert m1.save() == m2.save()

def store_message(fd: typing.IO, data: bytes, expect_authentic: bool,
                  counter_jump=False) -> None:
    action = 0
    if expect_authentic:
        action |= 1
    if counter_jump:
        action |= 2
    assert len(data) == PACKET_SIZE
    fd.write(struct.pack("<BB", len(data), action))
    fd.write(data)

def main():
    with open("test_hmac433.bin", "wb") as fd:
        m = MAC(b"secret")
        # Authentic messages
        for i in range(300):
            packet = "msg{}".format(i).encode("ascii")
            data = m.encode_packet(packet)
            store_message(fd, data, True)
        # Corrupt message
        packet = "hello".encode("ascii")
        data = m.encode_packet(packet)
        data = b"H" + data[1:]
        store_message(fd, data, False)
        # Bad counter
        m.counter += 0x100
        data = m.encode_packet(packet)
        store_message(fd, data, False)
        m.counter -= 0x100
        # Good counter
        data = m.encode_packet(packet)
        store_message(fd, data, True)
        # Good counter jumps forward
        for i in range(5):
            packet = "jump{}".format(i).encode("ascii")
            m.counter += 0xff
            data = m.encode_packet(packet)
            store_message(fd, data, True)
        # Replay
        store_message(fd, data, False)
        packet = "replay".encode("ascii")
        data = m.encode_packet(packet)
        store_message(fd, data, True)
        store_message(fd, data, False)
        # The distant future.. the year 2000...
        m.counter += 1 << 60
        for i in range(5):
            packet = "futur{}".format(i).encode("ascii")
            data = m.encode_packet(packet)
            store_message(fd, data, True, counter_jump=(i == 0))

        m.counter += 1 << 60
        packet = "final".format(i).encode("ascii")
        data = m.encode_packet(packet)
        store_message(fd, data, False)
        store_message(fd, data, True, counter_jump=True)
    print("created test_hmac433.bin")


if __name__ == "__main__":
    main()
    sys.exit(pytest.main([__file__]))

