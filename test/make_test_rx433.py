
from readcode import read_edges_in_zip, read_edges
import math
import typing

SYMBOL_SIZE = 5
DATA_SIZE = 31

def synthesize_new_code(message: typing.List[int]) -> typing.List[typing.Tuple[float, bool]]:
    period = 0x200
    high = 0x100

    out: typing.List[typing.Tuple[float, bool]] = []
    out.append((0.0, False))

    def send_high(delta: float) -> None:
        assert delta > 0.0
        (previous, _) = out[-1]
        out.append((previous, True))
        out.append((previous + (delta / 1e6), False))

    def wait(delta: float) -> None:
        assert delta > 0.0
        (previous, _) = out[-1]
        out.append((previous + (delta / 1e6), False))

    # send start code: 101010101011
    for j in range(5):
        send_high(high)
        wait((period * 2) - high)

    for j in range(2):
        send_high(high)
        wait(period - high)

    for symbol in message:
        assert 0 <= symbol
        assert symbol < (1 << SYMBOL_SIZE)
        # send symbol
        for j in range(SYMBOL_SIZE):
            if symbol & (1 << (SYMBOL_SIZE - 1)):
                send_high(high)
                wait(period - high)
            else:
                wait(period)
            symbol = symbol << 1
        # end of symbol
        send_high(high)
        wait(period - high)

    # gap before allowing anything else
    wait(period * 2)
    return out


def main():
    recordings = [
        read_edges_in_zip("he_433_tests.zip", "20230409-0001-office-off-04022b83.csv", 1.0),
        read_edges_in_zip("he_433_tests.zip", "20230409-0002-bed3-on-00ad6496.csv", 1.0),
        synthesize_new_code(list(range(DATA_SIZE))),
        synthesize_new_code(list(reversed(range(DATA_SIZE)))),
    ]

    with open("test_rx433.txt", "wt") as fd:
        for edges in recordings:
            (start, _) = edges[0]
            for (time, high) in edges:
                if high:
                    fd.write("{:08x}\n".format(max(0,
                            int(math.floor((time - start) * 1e6)))))

if __name__ == "__main__":
    main()
