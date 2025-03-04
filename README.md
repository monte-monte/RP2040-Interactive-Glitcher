# Interactive glitcher that uses Raspberry Pi Pico

The aim of this project is to be able to bypass a read-out-protection of a common MCU. It consists of two main parts: an RP2040 firmware that is configured and controlled over USB serial and a python script that talks to RP2040 and validates the success of the glitch by trying to read a flash of an MCU by calling an external software (at this time it's stm8flash).

I was working on glitching of STM8, so that's what is currently ready for use out of the box. But the principle of operation is universal and you can easily add support for other MCUs.

Firmware for RP2040 uses PIO for precise timing. By default, it runs at 200Mhz and will provide 5ns resolution with a minimum delay of 10ns.
After boot, it awaits for the initial configuration for GPIOs, clock, output/input levels, etc. If no configuration was sent via serial it will fall back to the defaults. Then it will send its initial config to a serial and a word ``Ready``. At this stage, it will wait for one of the commands:

``#DELAY_AFTER_TRIGGER-DURATION_OF_A_GLITCH_PULSE`` (you can also use ``m`` before a number to send a number of milliseconds ``m500`` will be interpreted as ``500000000ns``)

``r`` - reboot RP2040

``f`` - reset prepared glitch (if it couldn't be triggered for some reason)

``t`` - manual trigger

``v`` - cycle VCC output

``g`` - cycle GND output

``p`` - cycle both

If it has received a command to set a glitch, for example ``#50000-1150``, it will ready the glitch and wait for a trigger, after the trigger it will countdown the delay in nanoseconds represented by the first number in a command then it will set an output GPIO to a predefined level and countdown the second delay, then set GPIO to a default level and wait for another command.

All of that is being taken care of by the python script ``glitcher.py``. You can run it with options set as command arguments or you can edit the ``config.json`` file and run it like this ``python3 glitcher.py -c config.json``.

Here are the settings you can (and should) change:

- ``serial`` - path to a serial port for RP2040
- ``delay_min``, ``delay_max`` - minimum and maximum values for a delay after trigger that will be scanned through by the script
- ``length_min``, ``length_max`` - minimum and maximum values for a glitch pulse that will be scanned through by the script
- ``step`` - incrementing step in ns for the delay
- ``freq`` - clock frequency in KHz to run RP2040 at
- ``in-gpio`` - GPIO number for trigger input
- ``out-gpio`` - GPIO number glitch pulse output
- ``in-dir`` - direction of input trigger 0/1 (FALLING/RISING edge)
- ``out-dir`` - direction of output glitch pulse 0/1 (LOW/HIGH)
- ``reset-gpio`` - GPIO number for reset output
- ``vcc-gpio`` - GPIO number for VCC control
- ``gnd-gpio`` - GPIO number for GND control
- ``power-cycle-mode`` - power cycle target after fail? (0 - No, 1 - VCC, 2 - GND, 3 - Both)
- ``power-cycle-delay`` - power cycle delay ms between OFF/ON
- ``trig`` - wait for input trigger? 0/1 (NO/YES)
- ``mod`` - use custom glitch mode
- ``impl`` - name of an implementation file to use
- ``model`` - chip model, if needed. In case of STM8 you can find the name format with ``stm8flash -l``.

## Building RP2040 firmware

For this you will need a copy of [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
``mkdir build && cd build``

``cmake -DPICO_SDK_PATH=/path/to/your/copy/of/picoSDK -DPICO_BOARD=your_board_name ..``

``make``

``picotool load -f pio_glitch.uf2`` or drag-n-drop to your RP2040 board in the file manager while in programming mode

## Building stm8flash

There are two versions of stm8flash as submodules here. One is the [upstream](https://github.com/vdudouyt/stm8flash) version, another is my [fork](https://github.com/monte-monte/stm8flash_retriever) with some changes that allow faster verifying of successful glitch and also ``-a`` READALL mode that will dump all flash once it detects that ROP is disabled.

``impl/stm8.py`` uses standard stm8flash, ``impl/stm8_mod.py`` uses *retriever* version. Using either of the versions requires to build them first:

``cd stm8flash_retriever``

``make``

## Adding another *implementation*

I developed this project to glitch an STM8 MCU, so that's what I have written for. If you want to use it with another MCU, or for example use another way of validation/download you can create a separate ``.py`` file in the ``impl`` folder. You will need to add two functions:
```Python
def validate(model, output_file, silent = False)
def download(model, output_file, silent = False)
```
As you may see one is for validation of a glitch, when you are looking for the exact timing that works with your MCU and setup. ``download`` function will run once a successful glitch is detected by the ``validation`` function, and it should contain code to read and save the memory you are trying to get from the MCU. If you have a new implementation for this please open a PR :)