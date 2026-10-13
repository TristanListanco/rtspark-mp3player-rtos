# Potentiometer volume setup

Use a 10 kohm linear potentiometer (B10K) and three jumper wires. Power off the
RT-Spark before wiring.

| Pot terminal | Board connection |
|---|---|
| Outer terminal | PMOD2 3.3 V |
| Wiper (normally center terminal on a bare pot) | PMOD2 PF6 / Arduino A0 |
| Other outer terminal | PMOD2 GND |

For the laboratory Figure 1.2 circuit, put its 100 ohm resistor between the low
outer terminal and GND. Direct GND also works with the endpoint dead zones.
For a three-pin module, use its VCC, SIG and GND markings; order varies.
Never connect the pot supply or signal to 5 V.

```text
3.3 V ----+
          |
      [10 kohm] <---- wiper ---- PF6 / A0
          |
GND ------+
```

Use the top-right PMOD2 connector marked Type 1A/3A, with the board upright and
USB connectors at the bottom. The analog row is, left to right:

```text
3.3V   GND   PF4   PF5   PF6/A0   PF7
```

Confirm orientation using the [official RT-Thread pin map](https://github.com/RT-Thread/rt-thread/blob/master/bsp/stm32/stm32f407-rt-spark/applications/arduino_pinout/Rt-spark_Rtduino_Pin_Map.drawio.png)
and the [board schematic, expansion connector sheet](https://www.rt-thread.org/document/site/rt-thread-version/rt-thread-standard/hw-board/spark-1/SCH_Spark-1_V1_0.pdf).
PF6 uses ADC3 channel 4 in analog mode without pull resistors, 12-bit resolution,
21 MHz ADC clock, and 480-cycle sampling. The existing board initialization
already supplies these settings.

## Firmware behavior

The knob controls volume; KEY_UP on PC5 pauses/resumes, B1-B4 still select songs,
and the LED state indicators remain in use. KEY_LEFT/KEY_RIGHT no longer adjust
volume. At boot, the knob is read before auto-play so minimum is silent. If the
initial ADC read fails, playback is muted until a successful volume-task read.
Later failed reads retain the previous volume.

Every 100 ms, eight ADC samples are averaged. Counts at or below 80 map to 0%,
and counts at or above 4000 map to 100%; this includes endpoint dead zones and
accommodates the handout's 100 ohm resistor. The existing 2% hysteresis suppresses
small changes, while 0% and 100% are always reachable. Volume is mapped to codec
digital attenuation over 0 to -96 dB, with explicit mute at 0%.

## Build, flash and verify

1. Build: `pio run -e rtspark`.
2. With the board connected to USB-DBG, flash: `pio run -e rtspark -t upload`.
3. Open UART: `pio device monitor -b 115200`.
4. Start with the knob low, then turn it up. Check the LCD volume bar and UART
   messages such as `Volume: 50% (pot ADC: 2040)`; listen for volume to increase.
5. At minimum, check 0% and silence. At maximum, check 100%. Press KEY_UP twice
   to check pause/resume, and check B1-B4 song selection and LED indications.

If rotation is reversed, power off and swap the two outer pot wires. If volume
jumps, check the common ground and the wiper connection; keep jumper wires short.
An optional 100 nF capacitor from PF6 to GND near the header can reduce noise.
With a multimeter, the wiper should sweep approximately 0 to 3.3 V relative to
GND. A disconnected wiper floats and cannot be reliably detected in software.
If the measured endpoints do not reach 0%/100%, adjust `kPotRawMin`/`kPotRawMax`
in `src/main.cpp` to your observed UART ADC counts.

Hardware verification: the user confirmed that potentiometer volume control
works correctly on their physical RT-Spark board after flashing this firmware.
