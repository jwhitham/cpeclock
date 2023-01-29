#!/usr/bin/python3
# This is the server software
# Listen on port 433 for UDP packets
# Write them to the device /dev/tx433.

DEVICE = "/dev/tx433"
PORT = 433
ROOT = "/srv/root_services"
LIGHT_DATA_FILE = "/srv/root_services/lights.json"
SEND_INTERVAL = 2.5
# HUE_TARGET = ("192.168.2.11", 1990)

#CEILING_LIGHTS_PIN = 16         # relay output 2
OUTFRONT_LIGHTS_PIN = 12        # relay output 3

DAYLIGHT_TIME = 3600
DRYER_TIME = 3600 * 24

from twisted.internet.protocol import DatagramProtocol
from twisted.internet import reactor
import time, os, sys, subprocess, typing, json, socket
import RPi.GPIO as GPIO 

DEBUG = (os.getenv("HEDEBUG") == "HEDEBUG")


Short_Name = str
Light_Data = typing.Dict[str, str]
All_Light_Data = typing.Dict[Short_Name, Light_Data]

def get_all_light_data() -> All_Light_Data:
    try:
        return json.load(open(LIGHT_DATA_FILE, "rt"))
    except:
        return {}


class Server433(DatagramProtocol):
    def __init__(self) -> None:
        print ('startup %s' % DEVICE)
        print ('send interval is %1.2f seconds' % SEND_INTERVAL)

        self.last_day = None
        self.allow_send_at = 0.0
        self.send_queue: typing.List[int] = []
        self.daylight_off_after = 0.0
        self.dryer_off_after = 0.0
        self.tv_on_flag = False
        self.name_to_light_data_map = get_all_light_data()
        self.code_to_name_map: typing.Dict[int, str] = dict()
        for name in self.name_to_light_data_map:
            light_data = self.name_to_light_data_map[name]
            code = int(light_data["code"], 16)
            self.code_to_name_map[code] = name

        sys.stdout.flush()

        self.special_codes: typing.Dict[int, typing.Callable[[], bool]] = {
            self.get_code("daylight", True) : self.daylight_on,
            self.get_code("dryer", True) : self.dryer_on,
            self.get_code("tv", True) : self.tv_on,
            self.get_code("tv", False) : self.tv_off,
            self.get_code("ceiling", True) : self.ceiling_on,
            self.get_code("ceiling", False) : self.ceiling_off,
            self.get_code("outfront", True) : self.outfront_on,
            self.get_code("outfront", False) : self.outfront_off,

        }

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
    
    def ceiling_on(self) -> bool:
        print ('ceiling lights on')
        #GPIO.output(CEILING_LIGHTS_PIN, 1)
        return False

    def ceiling_off(self) -> bool:
        # No longer takes any action. Ceiling light pin control is always on.
        # print ('ceiling lights off')
        # GPIO.output(CEILING_LIGHTS_PIN, 0)
        return False

    def outfront_on(self) -> bool:
        print ('outfront lights on')
        GPIO.output(OUTFRONT_LIGHTS_PIN, 0)
        return True 

    def outfront_off(self) -> bool:
        print ('outfront lights off')
        #GPIO.output(OUTFRONT_LIGHTS_PIN, 1)
        return True 

    def daylight_on(self) -> bool:
        print ('send daylight on code: off handler reset')
        self.daylight_off_after = time.time() + DAYLIGHT_TIME - 10
        reactor.callLater(DAYLIGHT_TIME, self.daylight_off)
        current_time_struct = time.localtime(time.time())
        if not (6 <= current_time_struct.tm_hour <= 16):
            print ("not daytime - suppress light switch on")
            return True
        return False

    def dryer_on(self) -> bool:
        print ('send dryer on code: off handler reset')
        self.dryer_off_after = time.time() + DRYER_TIME - 10
        reactor.callLater(DRYER_TIME, self.dryer_off)
        return False

    def tv_on(self) -> bool:
        print ('send tv on code')
        self.tv_on_flag = True
        return False

    def tv_off(self) -> bool:
        print ('send tv off code')
        self.tv_on_flag = False
        reactor.callLater(10, self.tv_off_repeat)
        return False

    def tv_off_repeat(self, echo_counter = 3) -> None:
        if echo_counter <= 0:
            return
        if self.tv_on_flag:
            # turned back on
            return

        print ('send tv off code %d (%s)' % (echo_counter, time.asctime()))
        self.send_to_hue("tv auto")
        self.send_to_hue("tv off")
        self.send_to_hue("front auto")

        sys.stdout.flush()
        reactor.callLater(10, self.tv_off_repeat, echo_counter - 1)

    def send(self, number: int) -> None:
        # Copy the message to Hue
        self.send_copy_to_hue(number)

        # Send to Home Easy
        sp = self.special_codes.get(number, None)
        if sp is not None:
            if sp():
                # If special handler returned True, don't broadcast Home Easy code
                return

        self.send_queue.append(number)
        if len(self.send_queue) == 1:
            self.send_next()

    def send_copy_to_hue(self, number: int) -> None:
        (name, on_flag) = self.get_name(number)
        if name == "":
            return

        if on_flag:
            text = "{} on".format(name)
        else:
            text = "{} off".format(name)

        self.send_to_hue(text)

    def send_to_hue(self, text: str) -> None:
        pass

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

    def dryer_off(self, echo_counter = 3) -> None:
        if time.time() < self.dryer_off_after:
            return

        if echo_counter <= 0:
            return

        print ('dryer off at %s' % time.asctime())
        self.named_action("dryer off")
        sys.stdout.flush()
        reactor.callLater(10, self.dryer_off, echo_counter - 1)

    def daylight_off(self, echo_counter = 3) -> None:
        if time.time() < self.daylight_off_after:
            return

        if echo_counter <= 0:
            return

        print ('daylight light off at %s' % time.asctime())
        self.named_action("daylight off")
        sys.stdout.flush()
        reactor.callLater(10, self.daylight_off, echo_counter - 1)

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
    if 0 == subprocess.call(["/bin/cp", "-r", os.path.join(ROOT, "home_easy/kernel"), "/tmp/tx433"]):
        os.chdir("/tmp/tx433")
        os.environ["PWD"] = os.getcwd()
        if 0 == subprocess.call(["/usr/bin/make"]):
            print('Add driver for TX433')
            subprocess.call(["/sbin/insmod", "/tmp/tx433/tx433.ko"])

    os.chdir(ROOT)

    print ("load settings from %s" % LIGHT_DATA_FILE)
    all_light_data = get_all_light_data()
    print ('%u known lights' % len(all_light_data))

    print ("GPIO setup")
    GPIO.setmode(GPIO.BOARD)
    GPIO.setwarnings(False)
    #GPIO.setup(CEILING_LIGHTS_PIN, GPIO.OUT)
    #GPIO.output(CEILING_LIGHTS_PIN, 1)
    GPIO.setup(OUTFRONT_LIGHTS_PIN, GPIO.OUT)
    GPIO.output(OUTFRONT_LIGHTS_PIN, 1)

    print ("start server")
    reactor.listenUDP(PORT, Server433())
    reactor.run()

if __name__ == "__main__":
    main()

