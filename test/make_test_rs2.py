
from readcode import read_edges
import math
import typing

def main():
    recordings = [read_edges("c:/temp/20230506-0005.csv", 1)]

    with open("test_rs.txt", "wt") as fd:
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
