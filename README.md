Custom made alarm clock for a Adafruit Circuit Playground Express.
Controlled from a Raspberry Pi via a 433MHz radio link, with
Reed-Solomon error correction and HMAC authentication.

Two types of code are sent over the radio link. Firstly, "Home Easy"
codes which operate radio controlled sockets sold under
CH Byron's Home Easy brand. These were once widely available from
DIY stores such as B&Q. There is no authentication,
the protocol is very simple, and the sockets cannot acknowledge
receiving a code. Each socket can be set up to receive up to 4
different codes which allow it to be switched on or off remotely.
This is a legacy protocol to support a few old sockets that are still in use.

Secondly, "new codes" are my own design, intended to fit around
the capabilities of Reed Solomon encoding and the apparent limitations
of the 433MHz radio link. The design is based on experiments over a few days;
the protocol has error correction and authentication, but is still only one-way.

New codes consist of 31 (NC\_DATA\_SIZE) symbols of 5 bits
for a total of 155 bits. Over the air:

* each symbol begins with 11010, then
* five bits, sent as either 10 (if 1) or 00 (if 0), MSB first, then
* either the next symbol, or an end symbol, 10.

So, for example, the symbol 10101 would be sent as:

       110101000100010
       ^^^^^ start
            ^^ 1 (MSB)
              ^^ 0
                ^^ 1
                  ^^ 0
                    ^^ 1 (LSB)

This would be followed immediately by another symbol or 10.

The length of each transmitted 1 or 0 is NC\_PULSE i.e. 256 microseconds so
* 11 means "transmitter on" for 512 microseconds, and
* 10 means "transmitter on" for 256 microseconds and then "transmitter off" for 256 microseconds.
 
This may be easier to understand if you look at kernel/tx433.c, particularly transmit\_new\_code,
send\_high\_var and await.

The decoder is in rx433.c.
The interrupt handler is triggered by each 0 -> 1 transition. The intention of the start
code is to create a pattern of 0 -> 1 transitions which cannot occur elsewhere in a packet:
though in reality, because the radio link is noisy, transitions can be seen at any time.

The first few symbols might not be received, in which case the Reed Solomon code
is supposed to be able to recover the missing data. 10 symbols (50 bits) are used
for RS error correction. The remaining 21 symbols (105 bits) are split between
a HMAC code (56 bits) and the actual payload (48 bits) with one unused bit. The
error correcting symbols are interleaved with HMAC/payload symbols because this
allowed for better recovery if the first symbols were lost and the whole message
was offset.
