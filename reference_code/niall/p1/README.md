
# CSSE4011 Prac 1
### _Introduction to Zephyr RTOS and ~~Bootloader~~_

#### **Niall Waller - s45819397 - Partner B**

---
## Folder Structure
repo/mycode/apps
* p1
    * t1
        * boards
            * nucleo_l496zg.overlay
        * src
            * headers.h
            * main.c
        * CMakeLists.txt
        * prj.conf
    * t2
        * boards
            * nucleo_l496zg.overlay
        * src
            * headers.h
            * main.c
        * CMakeLists.txt
        * prj.conf
    * t3
        * boards
            * nucleo_l496zg.overlay
        * src
            * headers.h
            * main.c
        * CMakeLists.txt
        * prj.conf

---
## Design Task 1
### Functionality
Full functionality achieved. The program uses a thread to generate a random number 8 digits long every 2 seconds and then passes it to a message queue primitive where it is retrieved by another thread which displays it to the terminal. The random numbers are generated using a hardware entropy device.

### Instructions
1. Assuming access to the repo folder and device USB connection onto a linux terminal in VS Code.
2. You can open a minicom terminal to view serial output using 'minicom -D /dev/ttyACM*'.
3. To build the design task use 'west build -p -b nucleo_l496zg mycode/apps/p1/t1'.
4. To flash the design task onto the device use 'west flash --runner jlink'.
5. Watch the minicom terminal for serial outputs of the random number generation every 2 seconds.
6. To see thread behaviour run the build command with the additional flags '-- -DCONFIG_QEMU_ICOUNT=n -DCONFIG_THREAD_ANALYZER=y -DCONFIG_THREAD_ANALYZER_USE_PRINTK=y -DCONFIG_THREAD_ANALYZER_AUTO=y -DCONFIG_THREAD_ANALYZER_AUTO_INTERVAL=10 -DCONFIG_THREAD_NAME=y'.

---
## Design Task 2
### Functionality
Full functionality achieved. The program uses a thread to cycle through the 8 colours in order, storing the desired colour in a message queue where another thread accesses it and activates the required GPIO pins accordingly. The setup makes use of the given NAND gate chip as a driver, and also utilises a pwm pin to control the colour and brightness of the RGB LED.

### Instructions
#### Hardware
Refer to the [ST Nucleo-L496ZG Pin Diagram](https://os.mbed.com/platforms/ST-Nucleo-L496ZG/) and [74HC03 Datasheet](https://www.ti.com/lit/ds/symlink/sn74hc03.pdf?ts=1741926828114&ref_url=https%253A%252F%252Fau.mouser.com%252F) for pin labels and connections.
1. Use a ~220 ohm resistor between the chips VCC and each of the LEDs RGB terminals to pull them up to defualt high.
2. Connect the PWM pin (PA_0) to an input of three different NAND gates on the chip.
3. Connect the RED pin (PF_0), GREEN pin (PF_1) and BLUE pin (PF_2) to the other inputs.
4. Connect the outputs of each of the three NAND gates with its respective terminal on the RGB LED.
5. Connect the GND pin on the chip to a ground terminal on the board and the GND pin on the RGB LED.
6. Connect the VCC pin on the chip to a 3V3 or 5V terminal on the board.

#### Code
1. Assuming access to the repo folder and device USB connection onto a linux terminal in VS Code.
2. You can open a minicom terminal to view serial output using 'minicom -D /dev/ttyACM*'.
3. To build the design task use 'west build -p -b nucleo_l496zg mycode/apps/p1/t1'.
4. To flash the design task onto the device use 'west flash --runner jlink'.
5. Watch the LED cycle through 8 different colours.

---
## Design Task 3
### Functionality
Full functionality achieved. The program opens a command line interface shell with the user over the sereal uart connection. From here the user has access to a command that can display the system uptime, and a command to set and toggle two of the on board LEDs. The user will also be able to see configured log messages relevant to the interface and these functions.

#### Instructions
1. Assuming access to the repo folder and device USB connection onto a linux terminal in VS Code.
2. Open a minicom terminal to view serial output using 'minicom -D /dev/ttyACM*'.
3. To build the design task use 'west build -p -b nucleo_l496zg mycode/apps/p1/t1'.
4. To flash the design task onto the device use 'west flash --runner jlink'.
5. The command line interface shell with the board will load into the minicom terminal.
6. Type the command 'help' to get a list of commands you can use from this shell interface, including two custom ones:
   1. time: allows you to view the current uptime of the system.
   2. led: allows you to set and toggle two of the boards led's
   
---
# References
---
1: Zephyr Code (including examples).

_Manually searched through for similar functionality cases._
_Searched using 'grep -r' for function usage examples._


2: Zephyr Documentation.

_Able to manually search for desired outcomes leading to the required functions._


3: Zephyr Api.

_Provided greater detail on function use and implimentation as well as providing a linking to other useful functions._


4: https://docs.zephyrproject.org/latest/boards/st/nucleo_l496zg/doc/index.html

_Zephyr Nucleo-L496ZG overview documentation provided general knowledge._


5: https://www.st.com/resource/en/user_manual/dm00368330.pdf

_Datasheet for the Nucleo-L496ZG board provided general knowledge._


---
1.1: zephyr/tests/subsys/random/rng/src/main.c

_Only provided sample code of rng I could find._


1.2: https://docs.zephyrproject.org/latest/kernel/services/index.html

_Guide on different methods of communication between threads._


1.3: https://docs.zephyrproject.org/latest/doxygen/html/group__msgq__apis.html#ga54a5cdcaea2236c383ace433fedc0d39

_API documentation for creating and using message queues._


1.4: zephyr/doc/kernel/services/data_passing/message_queues.rst

_Additional information with examples of how to create and use message queueus._


1.5: https://docs.zephyrproject.org/latest/kernel/services/threads/index.html

_Guide to the creation and usage of threads._


1.6: https://docs.zephyrproject.org/latest/doxygen/html/group__random__api.html#details

_API documentation specifying functions that can generate randomness_.


1.7: https://docs.zephyrproject.org/latest/services/crypto/random/index.html

_Guide on random number generation._


---
2.1: https://www.youtube.com/watch?v=nHu3_K9OrMg

_General tutorial on how to set up GPIO and PWM pin access using device trees._
_The best way to use a GPIO pin as an output is to configure it as an LED with 'compatible = "gpio-leds";'._
_Code for accessing and using pins is depreciated (video is 3 years old)._


2.2: /zephyr/samples/basic/blinky

_Blinky provided a perfect example on how to use GPIO pins with 'GPIO_DT_SPEC_GET' and 'gpio_pin_configure_dt' and 'gpio_pin_toggle_dt'. This directed me to the 'gpio_dt_spec' datatype._


2.3: https://docs.zephyrproject.org/latest/doxygen/html/group__gpio__interface.html#gabfab69282fb99be119760436f2d18a9b

_Here I discovered 'gpio_pin_set_dt' and 'GPIO_DT_SPEC_GET_BY_IDX' as well as formalised my knowledge on the functions used by blinky._


2.4: build/zephyr/zephyr.dts

_Looked at blinkys compiled zephyr.dts device tree for example on how its on board LED devices are configured. Copied this format with aliases and an &led node reference in my .overlay file and then checked that it had correctly transferred to the device tree afterwards._
P

2.5: https://www.justfreetools.com/en/rgb-color

_Used to determine RGB requirements to produce the required colours._


2.6 https://os.mbed.com/platforms/ST-Nucleo-L496ZG/

_Detailed and easy to read pin diagram of the board, pin selection and referencing._


2.7 https://www.ti.com/lit/ds/symlink/sn74hc03.pdf?ts=1741926828114&ref_url=https%253A%252F%252Fau.mouser.com%252F

_Datasheet for the NAND gate chip assisted with wiring._


2.8 https://www.geeksforgeeks.org/enumeration-enum-c/

_Provided background knowledge and examples on the use of enums in C programming._


2.9 zephyr/samples/basic/blinky_pwm

_Example used and adapted to create pwm device tree overlay and access/set the pin._

---
3.1 https://docs.zephyrproject.org/latest/services/shell/index.html#shell-api

_Guide to using the zephyr shell._
_Gives a simple command handler implimentation._


3.2 https://docs.zephyrproject.org/apidoc/latest/group__shell__api.html

_Details the implimentation of functions used for the zephyr shell._


3.3 zephyr/samples/subsys/shell/shell_module

_Provided code examples for creating commands for the zephyr shell._


3.4 https://docs.zephyrproject.org/latest/kernel/services/timing/clocks.html

_Guide to kernal timing access._


3.5 https://docs.zephyrproject.org/apidoc/latest/group__clock__apis.html#gae3e992cd3257c23d5b26d765fcbb2b69

_Details the implimentation of functions used for kernal timing access and modification._


3.6 https://docs.zephyrproject.org/latest/services/logging/index.html#logging-kconfig

_Zephyr documentation guide on Logging._

---