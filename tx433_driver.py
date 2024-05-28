
import pigpio
import time

SYMBOL_SIZE   = 5      # 5 bits per symbol

NC_DATA_SIZE  = 31     # New codes: 31 base-32 symbols
NC_PULSE      = 0x100  # Timing for new code

class TX433_Driver:
    def __init__(self, pi: pigpio.pi, gpio_pin: int) -> None:
        self.__pi = pi
        self.__gpio_pin = gpio_pin
        self.__mask = 1 << gpio_pin

        self.__pi.set_mode(self.__gpio_pin, pigpio.OUTPUT)
        self.__pi.write(self.__gpio_pin, 0)
        self.__pi.wave_clear()

        self.__pulses: typing.List[pigpio.pulse] = []

    def __send_high_var(self, high_us: int, low_us: int) -> None:
        self.__pulses.append(pigpio.pulse(self.__mask, 0, high_us))
        self.__pulses.append(pigpio.pulse(0, self.__mask, low_us))

    def __await(self, low_us: int) -> None:
        self.__pulses.append(pigpio.pulse(0, 0, low_us))

    def __send_high(self, low_us: int) -> None:
        self.__send_high_var(220, low_us)

    def __send_zero(self) -> None:
        self.__send_high(1330)

    def __send_one(self) -> None:
        self.__send_high(320)

    def __start(self) -> None:
        self.__pi.wave_clear()
        self.__pulses.clear()

    def __finish(self) -> None:
        self.__pi.wave_add_generic(self.__pulses)
        self.__pulses.clear()
        wave_id = self.__pi.wave_create()
        try:
            self.__pi.wave_send_once(wave_id)
            while self.__pi.wave_tx_busy():
                time.sleep(0.01)
        finally:
            self.__pi.wave_delete(wave_id)
            self.__pi.wave_clear()

    def write_he(self, tx_code: int, attempts: int = 10) -> None:
        """Send Home Easy code."""
        self.__start()
        for i in range(attempts):
            self.__send_high(2700) # Start code
            j = 32
            while j > 0:
                j -= 1
                if (tx_code >> j) & 1:
                    self.__send_zero()
                    self.__send_one()
                else:
                    self.__send_one()
                    self.__send_zero()

            self.__send_high(10270) # End code and gap

        self.__finish()

    def write_nc(self, message: bytes) -> None:
        """Send new (long) code."""
        self.__start()
        for i in range(NC_DATA_SIZE):
            symbol = message[i]
            # start symbol: 11010
            self.__send_high_var(NC_PULSE * 2, NC_PULSE)
            self.__send_high_var(NC_PULSE, NC_PULSE)
            # send symbol
            for j in range(SYMBOL_SIZE):
                if symbol & (1 << (SYMBOL_SIZE - 1)):
                    # 10
                    self.__send_high_var(NC_PULSE, NC_PULSE)
                else:
                    # 00
                    self.__await(NC_PULSE * 2)
                symbol = symbol << 1
        # end of final symbol: 10
        self.__send_high_var(NC_PULSE, NC_PULSE)
        self.__finish()

    def write_energenie(self, tx_code: int, attempts: int = 10) -> None:
        """Send Energenie code."""
        self.__start()
        period = 850
        for i in range(attempts):
            for j in range(25):
                if (tx_code << j) & (1 << 23):
                    high = 620
                else:
                    high = 205
                self.__pulses.append(pigpio.pulse(self.__mask, 0, high))
                self.__pulses.append(pigpio.pulse(0, self.__mask, period - high))
            self.__pulses.append(pigpio.pulse(0, 0, period * 7))
        self.__finish()
