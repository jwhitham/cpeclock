
import pigpio
import time


class TX433_Driver:
    def __init__(self, pi: pigpio.pi, gpio_pin: int) -> None:
        self.__pi = pi
        self.__gpio_pin = gpio_pin
        self.__mask = 1 << gpio_pin

        self.__pi.set_mode(self.__gpio_pin, pigpio.OUTPUT)
        self.__pi.write(self.__gpio_pin, 0)

    def write_nc(self, code: bytes) -> None:
        pass

    def __send_high_var(self, us: int) -> None:
        self.__pi.wave_add_generic([
            pigpio.pulse(self.__mask, 0, us),
            pigpio.pulse(0, self.__mask, 0),
            ])

    def __await(self, us: int) -> None:
        self.__pi.wave_add_generic([
            pigpio.pulse(0, 0, us),
            ])

    def __send_high(self) -> None:
        self.__send_high_var(220)

    def __send_zero(self) -> None:
        self.__send_high()
        self.__await(1330)

    def __send_one(self) -> None:
        self.__send_high()
        self.__await(320)

    def write_he(self, tx_code: int, attempts: int = 10) -> None:
        """Send Home Easy code."""
        self.__pi.wave_clear()
        for i in range(attempts):
            self.__send_high() # Start code
            self.__await(2700)
            j = 32
            while j > 0:
                j -= 2
                bits = (tx_code >> j) & 3
                if bits == 0:
                    self.__send_one()
                    self.__send_zero()
                    self.__send_one()
                    self.__send_zero()
                elif bits == 1:
                    self.__send_one()
                    self.__send_zero()
                    self.__send_zero()
                    self.__send_one()
                elif bits == 2:
                    self.__send_zero()
                    self.__send_one()
                    self.__send_one()
                    self.__send_zero()
                else:
                    self.__send_zero()
                    self.__send_one()
                    self.__send_zero()
                    self.__send_one()

        self.__send_high() # End code
        self.__await(10270) # Gap
        wave_id = self.__pi.wave_create()
        self.__pi.wave_send_once(wave_id)
        while self.__pi.wave_tx_busy():
            time.sleep(0.01)
        self.__pi.wave_delete(wave_id)

