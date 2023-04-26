
from readcode import read_edges_in_zip, read_edges
import math


def main():
    recordings = [
        read_edges_in_zip("he_433_tests.zip", "20230409-0001-office-off-04022b83.csv", 1.0),
        read_edges_in_zip("he_433_tests.zip", "20230409-0002-bed3-on-00ad6496.csv", 1.0),
        read_edges("6f5902ac237024bdd0c176cb93063dc4.csv", 1e-3),
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
