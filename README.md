# esphomeyaml deployed IOT fleet for home

This repo tracks the various ESP32 and ESP8522 devices I have deployed about the 
house.

### Setup
In any case, you nee the venv setup properly
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

### `powermeter`
* wio_link (should move to a node)
* In the garage
* IR sensitive phototransistor
* sensor_filter translates pulses to kWh

#### TODO:
* light sensor to determine when light is off/on
* IR/contact sensor to see if garage is open or closed
* Motion sensor?

### Secrets

I expect a `secrets.yaml` file with all necessary secrets to be passed in the 
style of `Home Assistant`

### `.gitignore`

the `.gitignore` is the same scheme as the one from `Home Assistant` in that secrets are 
passed by reference from a `secrets.yaml` file that is being ignored.