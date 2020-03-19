# Smart meter reader

This repository contains code to read a smart meter adhering to the DSMR (Dutch Smart Meter Requirements).
It posts the information to mqtt.

It seems like the meter Sagemcom T210-D is able to supply enough current to keep this running.

The current implementation posts all data that the meter reports to a topic `devices/dsmr/dsmr-esp/status`.
It reports all the data that the meter mentioned above produces, plus some extra debug information.

## Requirements

The code is made to run on a Wemos D1 mini, and can be compiled by using platformio.

The [arduino-dsmr](https://github.com/matthijskooijman/arduino-dsmr/) library is vendored in with two patches.

- One patch for a small bug reported there: [#30](https://github.com/matthijskooijman/arduino-dsmr/issues/30)
- The other patch for disabling the manipulation of the enable pin, as it's not connected in my case

