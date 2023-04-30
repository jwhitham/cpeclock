
from hmac433 import HMAC433, SECRET_SIZE

def main() -> None:
    with HMAC433() as hmac:
        if len(hmac.secret_data) != SECRET_SIZE:
            print("Secret size is {} bytes - need {}".format(
                len(hmac.secret_data), SECRET_SIZE))
            return

        with open("../secret.h", "wt") as fd:
            fd.write("static const uint8_t SECRET_DATA[] = {\n");
            for b in hmac.secret_data:
                fd.write("0x{:02x}, ".format(b))
            fd.write("};\nstatic const size_t SECRET_SIZE = ");
            fd.write("0x{:02x};\n".format(SECRET_SIZE))

if __name__ == "__main__":
    main()

