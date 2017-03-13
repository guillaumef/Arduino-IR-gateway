# Arduino-IR-gateway
Arduino IR transceiver - register ir commands, prepare devices, activities and trigger.

Transforms an arduino in an IR receiver for learning codes and able to emit back any code learned.
The usb attached arduino make the whole processing available to the perl code 'irgateway'.
This arduino receiver code should be able to decode any IR remote. It implements its own decoding layer without the need of IRremote.
For the transmitter, IRremote does the job.

## Requirements

arduino-mk
arduino IRremote library (only used for emitter)

## Build
```shell
git clone ...
cd Arduino-IR-gateway
mkdir libraries
## get IRremote lib and move it/link it in libraries
make upload
```

## Configuration

Check the 'irgateway.conf' file.

The IR code learned is encoded like this:
```code
<repeat_nb>@<repeat_sleep_us>#<scale_us>*(<timing_high1>,<timing_low1>,<timing_high2>,...)
```
repeat_nb and repeat_sleep_us are not mandatory.


## Run

```shell
./irgateway [OPTION] [MODE]

Option: not mandatory
 -d <device>                   : override device target (/dev/ttyUSB0)
 -c <conf file>                : override configuration file
 -s                            : daemonize (for server mode)
 -l <log file>                 : daemon log file (for server mode)

Mode: mandatory
 -learn                        : learn everything missing in configuration
 -learn <device> <action>      : learn this device's action and save it
 -raw-learn                    : learn and print - don't save it

 -send <device> <action>       : send this device's action
 -raw-send <code>              : send raw code from argument
 -stdin-raw-send               : send raw code from stdin

 -activity <activity> <action> : run this activity

 -server <host> <port>         : server (0.0.0.0 54545)
```

And in server mode, you can use the irgateway-cli to avoid usb connection latency
```shell
./irgateway-cli [MODE]
Mode: mandatory
 send <device> <action>       : send this device's action
 raw-send <code>              : send raw code from argument
 activity <activity> <action> : run this activity
```
