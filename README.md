# AirGradient - Prometheus WiFi Sketch

## Fork changes
This fork from [geerlingguy](https://github.com/geerlingguy/airgradient-prometheus.git) is modified in the following ways:

- Send WiFi RSSI in dBm as part of the Prometheus scrape
- Display the WiFi RSSI in dBm on the screen
- Fix a couple of typos
- Add more detail in the metrics descriptions
- Configure BME280 and SGP30 sensors

## Original README.md below

[![CI](https://github.com/geerlingguy/airgradient-prometheus/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/geerlingguy/airgradient-prometheus/actions/workflows/ci.yml)

AirGradient has a [DIY air sensor](https://www.airgradient.com/diy/). I built one (actually, more than one). I want to integrate sensor data into my in-home Prometheus instance and graph the data in Grafana.

So I built this.

## How it Works

If you're using the official AirGradient Arduino sketch (`C02_PM_SHT_OLED_WIFI`), you can configure it to enable WiFi and send data to a remote server every 9 seconds (as it cycles through the display of PM2.5, CO2, temperature, and humidity values).

By default, it sends a small JSON payload to AirGradient's servers, and you can monitor the data via their service.

This sketch provides stats upon request to the prometheus server during scrapes. By changing the deviceId in this sketch, you can easily probe multiple AirGradient DIY sensors from Prometheus.

## How to Use

First, edit the configuration options in the `Config` section of `AirGradient-DIY.ino` to match your desired settings.

Make sure you enter the SSID and password for your WiFi network in the relevant variables, otherwise the sensor won't connect to your network.

Make sure you have the "AirGradient" library installed (in Arduino IDE, go to Tools > Manage Libraries...).

Upload the sketch to the AirGradient sensor ([instructions here](https://www.jeffgeerling.com/blog/2021/airgradient-diy-air-quality-monitor-co2-pm25#flashing)) using Arduino IDE, make sure it connects to your network, then test connecting to it by running this `curl` command:

```sh
$ curl http://airgradient-ip-address:9926/metrics
# HELP pm02 Particulate Matter PM2.5 value
# TYPE pm02 gauge
pm02{id="Airgradient"}6
# HELP rco2 CO2 value, in ppm
# TYPE rco2 gauge
rco2{id="Airgradient"}862
# HELP atmp Temperature, in degrees Celsius
# TYPE atmp gauge
atmp{id="Airgradient"}31.6
# HELP rhum Relative humidity, in percent
# TYPE rhum gauge
rhum{id="Airgradient"}38
```

Once you've verified it's working, configure Prometheus to scrape the sensor's endpoint: `http://sensor-ip:9926/metrics`.

Example job configuration in `prometheus.yml`:

```yaml
scrape_configs:
  - job_name: 'airgradient-bedroom'
    metrics_path: /metrics
    scrape_interval: 30s
    static_configs:
      - targets: ['airgradient-ip-address:9926']
```

### Static IP address

You can configure a static IP address for the sensor by uncommenting the `#define staticip` line and entering your own IP information in the following lines.

## License

MIT.

## Authors

  - [Jeff Geerling](https://www.jeffgeerling.com)
  - [Jordan Jones](https://github.com/kashalls)
