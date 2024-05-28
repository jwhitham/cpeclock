#!/usr/bin/python3
# This is the server software
# Listen on port 433 for UDP packets
# Write them to the GPIO device

from pathlib import Path
import typing
import pigpio
import tx433_driver

PORT = 433
ROOT = Path(__file__).parent
LIGHT_DATA_FILE: typing.Optional[Path] = Path("/srv/root_services/lights.json")
SEND_INTERVAL = 2.5
REPEATER_DELAY = 0.0
REPEATER_ADDRESS = ""
REPEATER_PORT = 0
HUE_ADDRESS = ""
HUE_PORT = 0
TIMER_ADDRESS = ""
TIMER_PORT = 0
ENERGENIE_MASK = 0xf000000f


from twisted.internet.protocol import DatagramProtocol # type: ignore
from twisted.internet import reactor # type: ignore
import time, os, sys, subprocess, json, socket, shutil, tempfile
import RPi.GPIO as GPIO  # type: ignore
import urllib.request
import argparse

Short_Name = str
Light_Data = typing.Dict[str, str]
All_Light_Data = typing.Dict[Short_Name, Light_Data]

CACHE_DATA: All_Light_Data = {}
CACHE_EXPIRY_TIME = 0.0
NC_DATA_SIZE = 31
NC_HEADER_SIZE = 2
NO_TIMER = "notimer "

def get_all_light_data() -> All_Light_Data:
    global CACHE_EXPIRY_TIME, CACHE_DATA, LIGHT_DATA_FILE
    t = time.time()
    if t < CACHE_EXPIRY_TIME:
        return CACHE_DATA

    if not LIGHT_DATA_FILE:
        CACHE_DATA = dict()
        CACHE_EXPIRY_TIME = t + (24 * 60 * 60)
        return CACHE_DATA

    print ("load settings from %s" % LIGHT_DATA_FILE, flush=True)
    CACHE_EXPIRY_TIME = t + 60
    try:
        with open(LIGHT_DATA_FILE, "rt") as reply:
            CACHE_DATA = json.load(reply)
            CACHE_EXPIRY_TIME = t + (24 * 60 * 60)
        return CACHE_DATA
    except Exception as e:
        print("get_all_light_data failed: {}".format(e))
        return CACHE_DATA

class AbstractData:
    def device_send(self, driver: tx433_driver.TX433_Driver) -> None:
        pass

    def network_send(self, address: str, port: int) -> None:
        pass

class NCData(AbstractData):
    def __init__(self, msg: bytes) -> None:
        self.msg = msg

    def device_send(self, driver: tx433_driver.TX433_Driver) -> None:
        print ("broadcast NC message at %s" % (time.asctime()))
        driver.write_nc(self.msg[NC_HEADER_SIZE:])

    def network_send(self, address: str, port: int) -> None:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.sendto(self.msg, (address, port))

class HEData(AbstractData):
    def __init__(self, number: int) -> None:
        self.number = number

    def device_send(self, driver: tx433_driver.TX433_Driver) -> None:
        print ("broadcast %08x at %s" % (self.number, time.asctime()))
        if (self.number & ENERGENIE_MASK) == ENERGENIE_MASK:
            driver.write_energenie((self.number & ~ENERGENIE_MASK) >> 4)
        else:
            driver.write_he(self.number)

    def network_send(self, address: str, port: int) -> None:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.sendto("{}{:08x}".format(NO_TIMER, self.number).encode("ascii"), (address, port))

class Server433(DatagramProtocol):
    def __init__(self, driver: tx433_driver.TX433_Driver) -> None:
        print ('startup')
        print ('send interval is %1.2f seconds' % SEND_INTERVAL)

        self.last_day = None
        self.allow_send_at = 0.0
        self.send_queue: typing.List[AbstractData] = []
        self.name_to_light_data_map = get_all_light_data()
        self.code_to_name_map: typing.Dict[int, str] = dict()
        for name in self.name_to_light_data_map:
            light_data = self.name_to_light_data_map[name]
            code = int(light_data["code"], 16)
            self.code_to_name_map[code] = name

        print(end="", flush=True)
        self.driver = driver

    def get_name(self, code: int) -> typing.Tuple[str, bool]:
        on_flag = False
        if code & 0x10:
            on_flag = True

        code &= 0xfffffff ^ 0x10
        name = self.code_to_name_map.get(code, "")
        return (name, on_flag)

    def get_code(self, name: str, on_flag: bool) -> int:
        light_data = self.name_to_light_data_map.get(name, None)
        if light_data is None:
            return -1 # No light with this name

        code = int(light_data["code"], 16)
        if on_flag:
            code |= 0x10
        return code
        
    def datagramReceived(self, data_bytes: bytes,
                host_port: typing.Tuple[str, int]) -> None:
        (host, port) = host_port

        data: typing.Optional[AbstractData] = None
        send_to_timer_service = False

        if ((len(data_bytes) == (NC_DATA_SIZE + NC_HEADER_SIZE))
        and data_bytes.startswith(b"NC")):
            # New Code transmitted by txnc433/udp.c or home_easy.py
            data = NCData(data_bytes)

        else:
            # Home Easy code. Possibly hex, or possibly a name
            try:
                data_text = data_bytes[:32].decode("ascii")
            except:
                data_text = ""

            send_to_timer_service = True
            if data_text.startswith(NO_TIMER):
                data_text = data_text[len(NO_TIMER):]
                send_to_timer_service = False

            try:
                number = int(data_text, 16)
            except:
                number = 0

            if 0 < number < (1 << 32):
                data = HEData(number)
            else:
                data = self.decode_named_action(data_text)

        if not data:
            print("Rejected message of size {}".format(
                len(data_bytes)), flush=True)
            return

        # Enqueue for device
        self.send_queue.append(data)
        if len(self.send_queue) == 1:
            self.send_next()

        # Recover the command in text form (if applicable)
        data_text = self.encode_named_action(data)

        # Copy to Hue service if any
        if data_text and HUE_ADDRESS and HUE_PORT:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.sendto(data_text.encode("ascii"), (HUE_ADDRESS, HUE_PORT))

        # Copy to timer service if any
        if (data_text and send_to_timer_service
        and TIMER_ADDRESS and TIMER_PORT):
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.sendto(data_text.encode("ascii"), (TIMER_ADDRESS, TIMER_PORT))
    
    def send_next(self) -> None:
        if len(self.send_queue) == 0:
            # nothing to send
            return
        
        t = time.time()
        if t >= self.allow_send_at:
            # can send one item
            data = self.send_queue.pop(0)
            self.allow_send_at = t + SEND_INTERVAL
           
            data.device_send(self.driver)
            print(end="", flush=True)

            # send to repeater if any
            if REPEATER_ADDRESS and REPEATER_PORT:
                reactor.callLater(REPEATER_DELAY,
                    data.network_send, REPEATER_ADDRESS, REPEATER_PORT)

            if len(self.send_queue) == 0:
                # done sending now
                return

            t = time.time()

        # reschedule: more to send, or not ready to send yet
        reactor.callLater(0.05 + max(0, self.allow_send_at - t), self.send_next)

    def decode_named_action(self, text: str) -> typing.Optional[HEData]:
        fields = text.split()
        if len(fields) != 2:
            return None
        on = (fields[-1] == "on")
        off = (fields[-1] == "off")
        if not (on or off):
            return None

        try:
            c1 = self.get_code(fields[0], on)
        except:
            c1 = -1
        if c1 < 0:
            print ('invalid named action (unknown light "{}")'.format(fields[0]))
            return None

        return HEData(c1)

    def encode_named_action(self, data: AbstractData) -> str:
        if isinstance(data, HEData):
            number = data.number
            (name, on_flag) = self.get_name(number)
            if name != "":
                if on_flag:
                    return "{} on".format(name)
                else:
                    return "{} off".format(name)
        return ""

def main() -> None:

    global PORT, LIGHT_DATA_FILE
    global REPEATER_PORT, REPEATER_ADDRESS
    global HUE_PORT, HUE_ADDRESS
    global TIMER_PORT, TIMER_ADDRESS

    parser = argparse.ArgumentParser(
        prog="home_easy",
        description="Transmitter for Home Easy / New Code messages; "
                    "intended to be run from systemd")
    parser.add_argument("--port", type=int, metavar="N",
        help="UDP port to listen on", default=PORT)
    parser.add_argument("--light-data-file", type=argparse.FileType('r'),
        metavar="FILE",
        help="JSON file containing name -> number translations "
             "for Home Easy lights, e.g. " + str(LIGHT_DATA_FILE))
    parser.add_argument("--repeater-address", type=str, metavar="IP",
        help="IP address of another home_easy transmitter "
             "acting as a repeater")
    parser.add_argument("--repeater-port", type=int, metavar="N",
        help="UDP port of another home_easy transmitter "
             "acting as a repeater")
    parser.add_argument("--hue-address", type=str, metavar="IP",
        help="IP address of hue_service")
    parser.add_argument("--hue-port", type=int, metavar="N",
        help="UDP port of hue_service (e.g. 1990)")
    parser.add_argument("--timer-address", type=str, metavar="IP",
        help="IP address of timer_service")
    parser.add_argument("--timer-port", type=int, metavar="N",
        help="UDP port of timer_service (e.g. 1987)")
    parser.add_argument("--gpio-pin", type=int, required=True,
        help="GPIO pin for 433MHz transmitter: "
            "24 is used with heating2; "
            "26 is used with pi3")
    args = parser.parse_args()

    PORT = args.port
    LIGHT_DATA_FILE = None
    REPEATER_PORT = args.repeater_port
    REPEATER_ADDRESS = args.repeater_address
    HUE_ADDRESS = args.hue_address
    HUE_PORT = args.hue_port
    TIMER_ADDRESS = args.timer_address
    TIMER_PORT = args.timer_port
    if args.light_data_file:
        LIGHT_DATA_FILE = Path(args.light_data_file.name).absolute()
        args.light_data_file.close()

    os.chdir(ROOT)

    all_light_data = get_all_light_data()
    print ('%u known lights' % len(all_light_data))

    pi = pigpio.pi()
    if not pi.connected:
        print('cannot open pigpio connection')
        sys.exit(1)

    driver = tx433_driver.TX433_Driver(pi, args.gpio_pin)

    print ("start server", flush=True)
    reactor.listenUDP(PORT, Server433(driver))
    reactor.run()

if __name__ == "__main__":
    main()

