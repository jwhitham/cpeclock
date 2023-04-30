
from hmac433 import HMAC433
import sys
import base64


def main() -> None:
    with HMAC433(b"\x01\x02\x03\x04") as hmac:
        print(hmac.counter)
        data = hmac.encode_packet(" ".join(sys.argv[1:]).encode("ascii"))
        print(base64.b16encode(data))

if __name__ == "__main__":
    main()

