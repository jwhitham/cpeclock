

import csv
import zipfile
import enum

TOLERANCE = 200 # microseconds

def read_edges(filename):
    with open(filename, "rt", newline="") as fd:
        return parse_csv(csv.reader(fd))

def read_edges_in_zip(zip_filename, rec_filename):
    with zipfile.ZipFile(zip_filename) as zf:
        with zf.open(rec_filename, "r") as fd:
            return parse_csv(csv.reader(fd.read().decode("ascii").splitlines()))

def parse_csv(rows):
    for (i, _) in enumerate(rows):
        if i >= 2:
            break

    low = True
    edges = []
    time = 0.0
    for row in rows:
        voltage = float(row[1])
        if low and (voltage > 1.6):
            time = float(row[0])
            edges.append((time, True))
            low = False
        elif (not low) and (voltage < 1.0):
            time = float(row[0])
            edges.append((time, False))
            low = True

    return edges

def decode_received(received):
    if len(received) != 64:
        return (0, "bad length {}".format(len(received)))

    code = 0
    for i in range(0, 64, 2):
        if received[i] and not received[i + 1]:
            # short then long -> bit 0
            code <<= 1
        elif received[i + 1] and not received[i]:
            # long then short -> bit 1
            code <<= 1
            code |= 1
        else:
            return (0, "invalid pair at time {}".format(i))
    
    return (code, "code {:08x}".format(code))

def debug(edges):
    gap = False
    received = []
    tolerance_required = 0.0
    for i in range(1, len(edges)):
        (start, high) = edges[i - 1]
        (finish, low) = edges[i]
        assert low != high
        microseconds = (finish - start) * 1e6
        description = ""
        if low:
            if abs(microseconds - 320) < TOLERANCE:
                description = "short" # send_one in Linux driver
                received.append(True)
                tolerance_required = max(tolerance_required, abs(microseconds - 320))
            elif abs(microseconds - 1330) < TOLERANCE:
                description = "long" # send_zero in Linux driver
                received.append(False)
                tolerance_required = max(tolerance_required, abs(microseconds - 1330))
            elif abs(microseconds - 2700) < TOLERANCE:
                description = "start"
                gap = False
                received.clear()
                tolerance_required = abs(microseconds - 2700)
            elif (not gap) and (microseconds > 4000):
                (_, description) = decode_received(received)
                description += " {:1.0f}".format(tolerance_required)
                gap = True
        else:
            if (not gap) and (abs(microseconds - 220) > TOLERANCE):
                description = "noise"
                gap = True


        print("{:6.3f}  {} for {:6.0f} {}".format(
            start,
            "high" if high else " low",
            microseconds,
            description))

def decode_edges(edges):
    received = []
    codes = []
    for i in range(1, len(edges)):
        (start, high) = edges[i - 1]
        (finish, low) = edges[i]
        assert low != high
        microseconds = (finish - start) * 1e6
        if low:
            if abs(microseconds - 320) < TOLERANCE:
                received.append(True)
            elif abs(microseconds - 1330) < TOLERANCE:
                received.append(False)
            elif abs(microseconds - 2700) < TOLERANCE:
                received.clear()
            elif microseconds > 4000:
                (value, _) = decode_received(received)
                if value:
                    codes.append(value)
                received.clear()
        else:
            if abs(microseconds - 220) > TOLERANCE:
                received.clear()

    return codes

class CodeLength(enum.Enum):
    SHORT = enum.auto()
    LONG = enum.auto()
    START = enum.auto()
    UNKNOWN = enum.auto()

class State(enum.Enum):
    RESET = enum.auto()
    RECEIVED_LONG = enum.auto()
    RECEIVED_SHORT = enum.auto()
    READY_FOR_BIT = enum.auto()

def alt_decode_edges(edges):
    state = State.RESET
    code_length = CodeLength.UNKNOWN
    bit_count = 0
    bit_data = 0
    codes = []

    old_time = 0.0
    for (time, high) in edges:
        if not high:
            continue

        units = ((time - old_time) * 1e6)
        old_time = time
        # 320 + 220 = 540 = short
        # 1330 + 220 = 1550 = long
        # 2700 + 220 = 2920 = start
        if 384 <= units < 768:              # 3 .. 5
            code_length = CodeLength.SHORT
        elif 1408 <= units < 1664:          # 11 .. 12
            code_length = CodeLength.LONG
        elif 2816 <= units < 3072:          # 22 .. 23
            code_length = CodeLength.START
        else:
            code_length = CodeLength.UNKNOWN

        if code_length == CodeLength.START:
            state = State.READY_FOR_BIT
            bit_count = 0
            bit_data = 0
        elif code_length == CodeLength.SHORT:
            if state == State.READY_FOR_BIT:
                # short
                state = State.RECEIVED_SHORT
            elif state == State.RECEIVED_LONG:
                # long then short -> bit 1
                bit_data = (bit_data << 1) | 1
                bit_count += 1
                if bit_count >= 32:
                    codes.append(bit_data)
                    state = State.RESET
                else:
                    state = State.READY_FOR_BIT
            else:
                # error
                state = State.RESET
        elif code_length == CodeLength.LONG:
            if state == State.READY_FOR_BIT:
                # long
                state = State.RECEIVED_LONG
            elif state == State.RECEIVED_SHORT:
                # short then long -> bit 0
                bit_data = (bit_data << 1) | 0
                bit_count += 1
                if bit_count >= 32:
                    codes.append(bit_data)
                    state = State.RESET
                else:
                    state = State.READY_FOR_BIT
            else:
                # error
                state = State.RESET

    return codes

def main():
    edges1 = read_edges_in_zip("he_433_tests.zip", "20230409-0001-office-off-04022b83.csv")
    edges2 = read_edges_in_zip("he_433_tests.zip", "20230409-0002-bed3-on-00ad6496.csv")

    codes11 = decode_edges(edges1)
    codes12 = alt_decode_edges(edges1)
    assert set(codes11) == set([0x04022b83])
    assert set(codes12) == set([0x04022b83])
    codes21 = decode_edges(edges2)
    codes22 = alt_decode_edges(edges2)
    assert set(codes21) == set([0x00ad6496])
    assert set(codes22) == set([0x00ad6496])
    assert len(codes12) >= len(codes11)
    assert len(codes22) >= len(codes21)
    print("ok {} {}".format(len(codes12), len(codes22)))

if __name__ == "__main__":
    main()
