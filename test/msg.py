
from hmac433 import HMAC433
import sys
import base64
import os


TX433 = "/dev/tx433"

def main() -> None:
    unencoded = " ".join(sys.argv[1:]).encode("ascii")
    if len(unencoded) == 0:
        print("No message")
        return
    if len(unencoded) > (192 // 8):
        print("Message is too long, max 192 bits")
        return

    with HMAC433() as hmac:
        data = hmac.encode_packet(unencoded)
        print("Counter becomes: {:08x}".format(hmac.counter))

    encoded = base64.b16encode(data)
    print("To transmit:", encoded.decode("ascii"))
    encoded += b"\n"
    if os.path.exists(TX433):
        try:
            with open(TX433, "wb") as fd:
                fd.write(encoded)
            print("Ok")
        except Exception:
            print("Unable to write to", TX433)
    else:
        print("No device")

if __name__ == "__main__":
    main()

