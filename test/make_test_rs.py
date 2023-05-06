
from readcode import read_edges
import math
import typing

def main():
    recordings = [
        read_edges("rstest1.csv", 1e-3),
        read_edges("rstest2.csv", 1e-3),
        read_edges("rstest3.csv", 1e-3),
        read_edges("rstest4.csv", 1e-3) + [(i * 0.001, True) for i in range(100)],
        read_edges("rstest5.csv", 1e-3),
    ]

    with open("test_rs.txt", "wt") as fd:
        for edges in recordings:
            fd.write("{:08x}\n".format(0))
            (start, _) = edges[0]
            for (time, high) in edges:
                if high:
                    fd.write("{:08x}\n".format(1 + max(0,
                            int(math.floor((time - start) * 1e6)))))

if __name__ == "__main__":
    main()
