# esphomeyaml deployed IOT fleet for home

This repo tracks the various ESP32 and ESP8622 devices I have deployed about the 
house.

### Setup

I've switched to a docker setup, so just `. aliases` and you'll have access to the functions and images 
without needing all the venv stuff I stared using. But for posterity:

In any case, you need the venv setup properly
```bash
. venv/bin/activate
```
After that,  

### Use
```bash
esphomeyaml powermeter.yaml compile # optional
esphomeyaml powermeter.yaml run # uploads and connects you to the running node
```

## Nodes

### `snek`
* wio_link (the 6 port bigger one)
* Uptime (just to generate some internal noise and count restarts)
* one internal DHT22 and two external DHT11
* Humidity 
* Temperature (in C)

#### TODO:
* RGB LCD display
* upgrade all sensors to at least DHT22
* move to two internal sensors, one external
* when the new enclosure comes, add basic fan control to ventilate if too humid or too hot
* setup LED strip control of the new enclosure, plus external buttons

### `powermeter`
* wio_link (should move to a node)
* In the garage
* IR sensitive phototransistor
* sensor_filter translates pulses to kWh

#### TODO:
* light sensor to determine when light is off/on
* IR/contact sensor to see if garage is open or closed
* Motion sensor?

### `plants`
* Adafruit ESP32 Feather
* LiPo battery powered, solar panel charging
* deep sleep setup, 2min on, 8min sleep. (change to 1m on, 1h sleep)
* uses MQTT rather than the API as that works better with deep sleep
* (eventually setup the soil moisture sensors with power switching)

### Secrets

I expect a `secrets.yaml` file with all necessary secrets to be passed in the 
style of `Home Assistant`

### `.gitignore`

the `.gitignore` is the same scheme as the one from `Home Assistant` in that secrets are 
passed by reference from a `secrets.yaml` file that is being ignored.
