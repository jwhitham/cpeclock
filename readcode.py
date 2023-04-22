

import csv
import zipfile

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

def main():
    codes = decode_edges(read_edges_in_zip("he_433_tests.zip", "20230409-0001-office-off-04022b83.csv"))
    assert set(codes) == set([0x04022b83])
    codes = decode_edges(read_edges_in_zip("he_433_tests.zip", "20230409-0002-bed3-on-00ad6496.csv"))
    assert set(codes) == set([0x00ad6496])
    print("ok")
    #with zipfile.ZipFile("he_433_tests.zip") as zf:
    #edges = read_edges("20230409-0002-bed3-on-00ad6496.csv")
    #codes = decode_edges(edges)
    #print(codes)
    edges = read_edges_in_zip("he_433_tests.zip", "20230409-0002-bed3-on-00ad6496.csv")
    debug(edges)

if __name__ == "__main__":
    main()
