
from readcode import read_edges_in_zip, read_edges
import math
import typing

SYMBOL_SIZE = 5
DATA_SIZE = 31
NC_PULSE = 0x100

TEST_CODE_1 = [6, 23, 31, 29, 26, 21, 1, 18, 1, 4, 20, 27, 22, 27, 29, 16, 9, 22, 25, 28, 20, 13, 15, 18, 16, 21, 17, 31, 1, 1, 2]
TEST_CODE_2 = [13, 17, 20, 16, 20, 14, 7, 29, 15, 29, 17, 1, 30, 20, 7, 7, 0, 28, 29, 1, 26, 14, 20, 22, 21, 28, 7, 21, 2, 28, 11]
TEST_CODE_3 = [31, 20, 5, 31, 8, 31, 0, 18, 14, 12, 29, 21, 22, 7, 9, 17, 8, 6, 8, 4, 9, 29, 16, 1, 24, 18, 27, 5, 23, 29, 31]
TEST_CODE_4 = [0, 28, 31, 29, 5, 30, 30, 20, 14, 12, 13, 4, 9, 12, 24, 10, 23, 4, 24, 29, 24, 15, 2, 17, 15, 5, 29, 0, 4, 17, 0]

def synthesize_new_code(message: typing.List[int]) -> typing.List[typing.Tuple[float, bool]]:
    out: typing.List[typing.Tuple[float, bool]] = []
    out.append((0.0, False))
    assert len(message) == DATA_SIZE

    def send_high(delta: float) -> None:
        assert delta > 0.0
        (previous, _) = out[-1]
        out.append((previous, True))
        out.append((previous + (delta / 1e6), False))

    def wait(delta: float) -> None:
        assert delta > 0.0
        (previous, _) = out[-1]
        out.append((previous + (delta / 1e6), False))

    for symbol in message:
        assert 0 <= symbol
        assert symbol < (1 << SYMBOL_SIZE)
        # start of symbol: 11010
        send_high(NC_PULSE * 2)
        wait(NC_PULSE)
        send_high(NC_PULSE)
        wait(NC_PULSE)
        # send symbol
        for j in range(SYMBOL_SIZE):
            if symbol & (1 << (SYMBOL_SIZE - 1)):
                send_high(NC_PULSE)
                wait(NC_PULSE)
            else:
                wait(NC_PULSE * 2)
            symbol = symbol << 1

    # end of final symbol: 10
    send_high(NC_PULSE)
    wait(NC_PULSE)
    return out


def main():
    recordings = [
        read_edges_in_zip("he_433_tests.zip", "20230409-0001-office-off-04022b83.csv", 1.0),
        read_edges_in_zip("he_433_tests.zip", "20230409-0002-bed3-on-00ad6496.csv", 1.0),
        synthesize_new_code(TEST_CODE_1),
        synthesize_new_code(TEST_CODE_2),
        synthesize_new_code(TEST_CODE_3),
        synthesize_new_code(TEST_CODE_4),
        read_edges("test5.csv", 1e-3),
        read_edges("test6.csv", 1e-3),
        read_edges("test7.csv", 1e-3),
        [(0.000, False), (0.003, True), (0.005, False)],
        # test7.csv - A spurious '1' bit during the start code causes the whole message to be misinterpreted.
    ]

    with open("test_rx433.txt", "wt") as fd:
        time = offset = 0.0
        for edges in recordings:
            (start, _) = edges[0]
            for (time, high) in edges:
                if high:
                    fd.write("{:08x}\n".format(max(0,
                            int(math.floor((offset + time - start) * 1e6)))))
            offset += time - start

if __name__ == "__main__":
    main()
