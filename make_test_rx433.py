
from readcode import read_edges_in_zip
import math


def main():
    edges1 = read_edges_in_zip("he_433_tests.zip", "20230409-0001-office-off-04022b83.csv")
    edges2 = read_edges_in_zip("he_433_tests.zip", "20230409-0002-bed3-on-00ad6496.csv")

    with open("test_rx433.txt", "wt") as fd:
        for edges in [edges1, edges2]:
            for (time, high) in edges:
                if high:
                    fd.write("{:08x}\n".format(max(0, int(math.floor(time * 1e6)))))

if __name__ == "__main__":
    main()
