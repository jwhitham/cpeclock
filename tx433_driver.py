
import pigpio
import time


class TX433_Driver:
    def __init__(self, pi: pigpio.pi, gpio_pin: int) -> None:
        self.__pi = pi
        self.__gpio_pin = gpio_pin
        self.__mask = 1 << gpio_pin

        self.__pi.set_mode(self.__gpio_pin, pigpio.OUTPUT)
        self.__pi.write(self.__gpio_pin, 0)

        self.__pulses: typing.List[pigpio.pulse] = []

    def write_nc(self, code: bytes) -> None:
        pass

    def __send_high(self, low_us: int) -> None:
        self.__pulses.append(pigpio.pulse(self.__mask, 0, 220))
        self.__pulses.append(pigpio.pulse(0, self.__mask, low_us))

    def __send_zero(self) -> None:
        self.__send_high(1330)

    def __send_one(self) -> None:
        self.__send_high(320)

    def write_he(self, tx_code: int, attempts: int = 10) -> None:
        """Send Home Easy code."""
        self.__pi.wave_clear()
        self.__pulses.clear()
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

        self.__pi.wave_add_generic(self.__pulses)
        wave_id = self.__pi.wave_create()
        self.__pi.wave_send_once(wave_id)
        while self.__pi.wave_tx_busy():
            time.sleep(0.01)
        self.__pi.wave_delete(wave_id)
