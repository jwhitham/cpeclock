#!/usr/bin/python3
# This is the server software
# Listen on port 433 for UDP packets
# Write them to the device /dev/tx433.

DEVICE = "/dev/tx433"
PORT = 433
ROOT = "/srv/home_easy"
LIGHT_DATA_URL = "http://192.168.2.11/cgi-bin/lights_json"
SEND_INTERVAL = 2.5
SEND_DELAY = 1.0


from twisted.internet.protocol import DatagramProtocol # type: ignore
from twisted.internet import reactor # type: ignore
import time, os, sys, subprocess, typing, json, socket
import RPi.GPIO as GPIO  # type: ignore
import urllib.request

DEBUG = (os.getenv("HEDEBUG") == "HEDEBUG")

Short_Name = str
Light_Data = typing.Dict[str, str]
All_Light_Data = typing.Dict[Short_Name, Light_Data]

CACHE_DATA: All_Light_Data = {}
CACHE_EXPIRY_TIME = 0.0

def get_all_light_data() -> All_Light_Data:
    global CACHE_EXPIRY_TIME, CACHE_DATA
    t = time.time()
    if t < CACHE_EXPIRY_TIME:
        return CACHE_DATA

    CACHE_EXPIRY_TIME = t + 60
    try:
        req = urllib.request.Request(url=LIGHT_DATA_URL)
        r = urllib.request.urlopen(req)
        reply = r.read().decode("utf-8")
        CACHE_DATA = json.loads(reply)
        CACHE_EXPIRY_TIME = t + (24 * 60 * 60)
        return CACHE_DATA
    except Exception as e:
        print("get_all_light_data failed: {}".format(e))
        return CACHE_DATA


class Server433(DatagramProtocol):
    def __init__(self) -> None:
        print ('startup %s' % DEVICE)
        print ('send interval is %1.2f seconds' % SEND_INTERVAL)

        self.last_day = None
        self.allow_send_at = 0.0
        self.send_queue: typing.List[int] = []
        self.name_to_light_data_map = get_all_light_data()
        self.code_to_name_map: typing.Dict[int, str] = dict()
        for name in self.name_to_light_data_map:
            light_data = self.name_to_light_data_map[name]
            code = int(light_data["code"], 16)
            self.code_to_name_map[code] = name

        sys.stdout.flush()


        try:
            self.device = open(DEVICE, "wt")
        except:
            print('cannot open device')
            sys.exit(1)

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
        
    def datagramReceived(self, data_bytes: bytes, host_port: typing.Tuple[str, int]) -> None:
        (host, port) = host_port
        try:
            data = data_bytes[:32].decode("ascii")
        except:
            data = ""
        try:
            number = int(data, 16)
        except:
            number = 0

        if not (0 < number < (1 << 32)):
            self.named_action(data)
            return

        # Data for broadcast
        self.send(number)
    
    def send(self, number: int) -> None:
        self.send_queue.append(number)
        if len(self.send_queue) == 1:
            t = time.time() + SEND_DELAY
            self.allow_send_at = max(self.allow_send_at, t)
            self.send_next()

    def send_next(self) -> None:
        if len(self.send_queue) == 0:
            # nothing to send
            return
        
        t = time.time()
        if t >= self.allow_send_at:
            # can send one item
            number = self.send_queue.pop(0)
            self.allow_send_at = t + SEND_INTERVAL
            
            print ("broadcast %08x at %s" % (number, time.asctime()))
            self.device.write("%08x\n" % number)
            self.device.flush()
            sys.stdout.flush()

            if len(self.send_queue) == 0:
                # done sending now
                return

            t = time.time()

        # reschedule: more to send, or not ready to send yet
        reactor.callLater(0.05 + max(0, self.allow_send_at - t), self.send_next)

    def named_action(self, text: str) -> None:
        fields = text.split()
        if len(fields) != 2:
            print ('invalid named action (%u fields)' % len(fields))
            return
        on = (fields[-1] == "on")
        off = (fields[-1] == "off")
        if not (on or off):
            print ('invalid named action (not "on" or "off")')
            return

        try:
            c1 = self.get_code(fields[0], on)
        except:
            print ('invalid named action (get_code error)')
            return
        if c1 < 0:
            print ('invalid named action (unknown light)')
            return
        try:
            self.send(c1)
        except:
            print ('invalid named action (send error)')
            return
            
def main() -> None:
    sys.stdout = sys.stderr = open("/tmp/home_easy.log", "wt")
    os.chdir(ROOT)

    print ('Compile driver for TX433')
    subprocess.call(["/sbin/rmmod", "tx433"], stderr=subprocess.DEVNULL)
    if 0 == subprocess.call(["/bin/cp", "-r", os.path.join(ROOT, "kernel"), "/tmp/tx433"]):
        os.chdir("/tmp/tx433")
        os.environ["PWD"] = os.getcwd()
        if 0 == subprocess.call(["/usr/bin/make"]):
            print('Add driver for TX433')
            subprocess.call(["/sbin/insmod", "/tmp/tx433/tx433.ko"])

    os.chdir(ROOT)

    print ("load settings from %s" % LIGHT_DATA_URL)
    all_light_data = get_all_light_data()
    print ('%u known lights' % len(all_light_data))

    print ("GPIO setup")
    GPIO.setmode(GPIO.BOARD)
    GPIO.setwarnings(False)

    print ("start server")
    reactor.listenUDP(PORT, Server433())
    reactor.run()

if __name__ == "__main__":
    main()

