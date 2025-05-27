
# CSSE4011 Prac 2
### _Sensor Processing, Logging and Viewing_

#### **Niall Waller - s45819397 - Partner B**

---
## Folder Structure
```
mycode/p2
├── README.md
├── t1
│   ├── mylib_clock
│   │   ├── CMakeLists.txt
│   │   ├── Kconfig
│   │   ├── include
│   │   │   └── mylib_clock_rtcc.h
│   │   ├── src
│   │   │   └── mylib_clock_rtcc.c
│   │   └── zephyr
│   │       └── module.yml
│   └── tapp
│       ├── CMakeLists.txt
│       ├── prj.conf
│       └── src
│           └── main.c
├── t2
│   ├── mylib_sensors
│   │   ├── CMakeLists.txt
│   │   ├── Kconfig
│   │   ├── boards
│   │   │   └── sltb004a.overlay
│   │   ├── include
│   │   │   ├── mylib_sensors_bmp280.h
│   │   │   ├── mylib_sensors_si1133.h
│   │   │   └── mylib_sensors_si7021.h
│   │   ├── src
│   │   │   ├── board_init.c
│   │   │   ├── mylib_sensors_bmp280.c
│   │   │   ├── mylib_sensors_si1133.c
│   │   │   └── mylib_sensors_si7021.c
│   │   └── zephyr
│   │       └── module.yml
│   └── tapp
│       ├── CMakeLists.txt
│       ├── boards
│       │   └── sltb004a.overlay
│       ├── prj.conf
│       └── src
│           └── main.c
├── t3
│   ├── mylib_shell
│   │   ├── CMakeLists.txt
│   │   ├── Kconfig
│   │   ├── include
│   │   │   └── mylib_shell_globalsh.h
│   │   ├── src
│   │   │   ├── mylib_shell_clock.c
│   │   │   └── mylib_shell_sensors.c
│   │   └── zephyr
│   │       └── module.yml
│   └── tapp
│       ├── CMakeLists.txt
│       ├── boards
│       │   └── sltb004a.overlay
│       ├── prj.conf
│       └── src
│           └── main.c
├── t4
│   ├── mylib_sampling
│   │   ├── CMakeLists.txt
│   │   ├── Kconfig
│   │   ├── boards
│   │   │   └── sltb004a.overlay
│   │   ├── include
│   │   │   ├── mylib_sampling_button.h
│   │   │   ├── mylib_sampling_json.h
│   │   │   └── mylib_sampling_thread.h
│   │   ├── src
│   │   │   ├── mylib_sampling_button.c
│   │   │   ├── mylib_sampling_json.c
│   │   │   ├── mylib_sampling_shell.c
│   │   │   └── mylib_sampling_thread.c
│   │   └── zephyr
│   │       └── module.yml
│   └── tapp
│       ├── CMakeLists.txt
│       ├── boards
│       │   └── sltb004a.overlay
│       ├── prj.conf
│       └── src
│           └── main.c
├── t5
│   ├── mylib_logfiles
│   │   ├── CMakeLists.txt
│   │   ├── Kconfig
│   │   ├── include
│   │   ├── src
│   │   └── zephyr
│   │       └── module.yml
│   └── tapp
│       ├── CMakeLists.txt
│       ├── boards
│       │   └── sltb004a.overlay
│       ├── prj.conf
│       └── src
│           └── main.c
└── t7
    ├── mylib_guipipe
    │   ├── CMakeLists.txt
    │   ├── Kconfig
    │   ├── boards
    │   │   └── sltb004a.overlay
    │   ├── include
    │   ├── src
    │   └── zephyr
    │       └── module.yml
    └── tapp
        ├── CMakeLists.txt
        ├── boards
        │   └── sltb004a.overlay
        ├── prj.conf
        └── src
            └── main.c
```
---
## Test App Project Building Instructions.
1. Assuming access to the repo folder and device USB connection onto a linux terminal in VS Code.
2. You can open a minicom terminal to view serial output using `minicom -D /dev/ttyACM*`.
3. To build the design task test app use `west build -p -b sltb004a mycode/apps/p1/t#/tapp`.
4. To flash the design task onto the device use `west flash --runner jlink`.
5. Done!

---
## Design Task 1
### Functionality
Full functionality achieved. Creates a library that allows reading from and writing to the hardware real time clock (and calendar).

### Instructions
To use the library:

1) Add this to your projects CMakeLists.txt file:
```
set(ENV{EXTRA_ZEPHYR_MODULES}
    "/home/directory/path/.../to/mylib_clock"
)
```
Note: More custom library modules can be added with a ";" seperator.

2) In your prj.conf set `CONFIG_MYLIB_CLOCK=y`.

3) Add header files.
```
#include <mylib_clock_rtcc.h>
```

You can now use the library. See the header file for function usage specifics.

Hint: Try building the t1/tapp project for an example!

---
## Design Task 2
### Functionality
Full functionality achieved for 3/4 sensor readings. Creates a library that uses threads to interface with hardware sensors allowing you to read from them without taking up main program resources.

### Instructions
To use the library:

1) Add this to your projects CMakeLists.txt file:
```
set(ENV{EXTRA_ZEPHYR_MODULES}
    "/home/directory/path/.../to/mylib_sensors"
)
```
Note: More custom library modules can be added with a ";" seperator.

2) In your prj.conf set `CONFIG_MYLIB_SENSORS=y`.

3) Add header files.
```
#include <mylib_sensors_bmp280.h>
#include <mylib_sensors_si1133.h>
#include <mylib_sensors_si7021.h>
```

4) Copy the boards folder (containing the sltb004a.overlay) file into your project directory.

You can now use the library. See the header file for function usage specifics.
Hint: Try building the t2/tapp project for an example!

---
## Design Task 3
### Functionality
Full functionality achieved. Shell commands can interface with both the clock and sensor libraries.

#### Instructions
To use the library (this one depends on other libraries!):

1) Add this to your projects CMakeLists.txt file:
```
set(ENV{EXTRA_ZEPHYR_MODULES} 
    "/home/directory/path/.../to/mylib_clock;/home/directory/path/.../to/mylib_sensors;/home/directory/path/.../to/mylib_shell;/home/directory/path/.../to/mylib_sampling"
)
```
Note: More libraries can be added with a ";" seperator.

2) In your prj.conf set:
```
CONFIG_MYLIB_CLOCK=y
CONFIG_MYLIB_SENSORS=y

CONFIG_MYLIB_SHELL=y
```

3) You will automatically have access to the shell commands. But if you want access to the shell instance used by the library called "sh_global":
```
#include <mylib_shell_globalsh>
```

4) Copy the boards folder (containing the sltb004a.overlay) file into your project directory.

You can now use the library. Type `help` into the CLI for command specifics.
Hint: Try building the t3/tapp project for an example!

---
## Design Task 4
### Functionality
Full functionality achieved. Continuous sampling can be configured using the shell. Contiuous sampling is started/stopped with a button on the board. Each sample is configured into a JSON string which can be viewed in the shell.

#### Instructions
To use the library (this one depends on other libraries!):

1) Add this to your projects CMakeLists.txt file:
```
set(ENV{EXTRA_ZEPHYR_MODULES} 
    "/home/directory/path/.../to/mylib_sensors;/home/directory/path/.../to/mylib_clock;/home/directory/path/.../to/mylib_samples"
)
```
Note: More custom library modules can be added with a ";" seperator.

2) In your prj.conf set:
```
CONFIG_MYLIB_SHELL=y
CONFIG_MYLIB_CLOCK=y
CONFIG_MYLIB_SENSORS=y

CONFIG_MYLIB_SAMPLING=y
```

3) Add header files if needed.

4) Copy the boards folder (containing the sltb004a.overlay) file into your project directory.

You can now use the library. Type `help` into the CLI for command specifics.
NOTE: THIS LIBRARYS TEST APP (TAPP) IS THE FINAL DEMO BUILD.

---
## Design Task 5
### Functionality
#### Instructions

---
## Design Task 7
### Functionality
#### Instructions

---
# References
---
## Task 1


1.1: https://www.silabs.com/documents/public/reference-manuals/efr32xg12-rm.pdf

_Wireless Gecko Reference Manual for the EFR32 chip used on the sltb004a thunderboard._
_Used to set and get from RTCC registers._

1.2: https://docs.zephyrproject.org/latest/doxygen/html/structrtc__time.html

_The rtc\_time struct reference documentation to make my library compatible with the zephyr RTC library._


1.3: modules/hal/silabs/gecko/emlib/inc/em_rtcc.h

_HAL library used to configure, get, and set from the RTCC registers._


1.3: modules/hal/silabs/gecko/emlib/inc/em_rtcc.h

_library used to check external low frequency clock was being used for RTCC._


1.4: https://edstem.org/au/courses/22383/discussion/2517796

_Ed post by Sam Kwort about file structure and CMakeLists.txt code for libraries._


1.5: https://docs.zephyrproject.org/latest/develop/modules.html#modules-without-west

_How to create modules(libraries)._


1.6: https://docs.zephyrproject.org/latest/develop/env_vars.html#env-vars

_Explains how to set the EXTRA\_ZEPHYR\_MODULES environment variable for adding your own modules_.
To set it for prac 2:
+ cp mycode/p2/.zepyrrc ~/.zephyrrc
+ source zephyr/zephyr-env.sh
Check with echo $EXTRA_ZEPHYR_MODULES


1.7: https://cmake.org/cmake/help/latest/index.html

_For understanding Cmake._


1.8: https://docs.zephyrproject.org/latest/build/cmake/index.html

_For understanding how zephyr uses CMake._


1.9: https://github.com/zephyrproject-rtos/example-application/blob/main/lib/custom/CMakeLists.txt

_Example application with a library._


1.10: https://docs.zephyrproject.org/latest/build/kconfig/index.html

_Guide to using Kconfig files for configs instead of the .config file._


1.11: https://www.tutorialspoint.com/c_standard_library/c_function_sscanf.htm

_Used to understand the sscanf() function for cmd argument parsing._

---
## Task 2

2.1: https://cmake.org/cmake/help/latest/manual/cmake-language.7.html#lists

_How to list multiple variables in a CMake environment variable (seperate with ';')._


2.2: https://www.silabs.com/documents/public/data-sheets/efr32mg12-datasheet.pdf

_EFR32MG12 Gecko Multi-Protocol Wireless SoC Family Data Sheet._


2.3: https://www.silabs.com/documents/public/schematic-files/BRD4166A-D00-schematic.pdf

_Board schematic._


2.4: https://edstem.org/au/courses/22383/discussion/2527683

_Sam's helpful ed post._


2.5: https://download.mikroe.com/documents/datasheets/Si1133.pdf

_Datasheet for si1133 (UV and Light sensor)._


2.6: https://www.silabs.com/documents/public/data-sheets/Si7021-A20.pdf

_Datasheet for si7021 (Humidity and Tempurature sensor)_.


2.7: https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp280-ds001.pdf

_Datasheet for bmp280 (Temp and Pressure sensor)._


2.8: https://docs.zephyrproject.org/latest/hardware/pinctrl/index.html

_How to name pins on the dt so that they work by default._
_Both the gas and environment sensors need the same I2C bus so have to replace the gas sensor as default or use dynamic pin control to get them to both work._


2.9: zephyr/samples/sensor/bme280

_Example usage of the bmp sensor._
_Got it working by using ed post #178 by Sam which involved setting the device tree right, using an initialising function to set the supply gpios and then removing humidity as the bmp280 doesnt have it unlike the bme280 the sample was origionally made for._


3.10: https://docs.zephyrproject.org/latest/hardware/peripherals/sensor/index.html

_How to program for sensors._


3.11: https://docs.zephyrproject.org/latest/doxygen/html/group__math__printing.html

_How to convert q31 values for printing._


3.11: https://docs.zephyrproject.org/apidoc/latest/group__spsc__lockfree.html

_SPSC message passing primitive API._

---
## Task 3

3.1: https://docs.zephyrproject.org/latest/services/shell/index.html#commands

_How to create and register shell commands with subcommands (its a heck of a lot easier than what I was doing previously)._

---
## Task 4

4.1: https://docs.zephyrproject.org/latest/doxygen/html/group__atomic__apis.html

_Atomic values._


4.2: ./zephyr/samples/basic/button

_Example of using a button._


4.3: https://github.com/zephyrproject-rtos/zephyr/blob/main/drivers/input/input_gpio_keys.c

_Driver for gpio-keys which the device tree has listed as compatible for the button nodes on my board. Its functions are used in the basic button example._


4.4: https://docs.zephyrproject.org/apidoc/latest/group__json.html

_Zephyr JSON API documentation._


4.5: ./zephyr/samples/net/secure_mqtt_sensor_actuator/src/mqtt_client.c

_Example of reading sensor values and encoding them in JSON format._


4.6: https://docs.zephyrproject.org/latest/kernel/memory_management/heap.html

_THANKYOU MALLOC._
_I needed to malloc for dynamically determined arrays._
---
## Task 5

5.1: 

---
## Task 7

7.1: 

---