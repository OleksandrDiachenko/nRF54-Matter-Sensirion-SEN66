# nRF54 Matter Sensirion SEN66

[![CI](https://github.com/OleksandrDiachenko/nRF54-Matter-Sensirion-SEN66/actions/workflows/ci.yml/badge.svg)](https://github.com/OleksandrDiachenko/nRF54-Matter-Sensirion-SEN66/actions/workflows/ci.yml)

A Matter air-quality accessory built with nRF Connect SDK for the nRF54LM20 DK,
nRF7002-EB II Wi-Fi shield, and Sensirion SEN66 sensor.

The device connects to a 2.4 GHz or 5 GHz Wi-Fi network and exposes supported
SEN66 measurements to Matter controllers, including Apple Home.

## Project status

The project is in the bootstrap phase. The first implementation milestone is a
commissionable Matter-over-Wi-Fi baseline before integrating the sensor.

## Hardware

- nRF54LM20 DK (`nrf54lm20a` or `nrf54lm20b` CPU application core)
- nRF7002-EB II shield
- Sensirion SEN66 connected over I2C at address `0x6B`
- iPhone with Apple Home

See [hardware wiring](docs/hardware.md) and the [roadmap](docs/roadmap.md).

## Engineering principles

- `main` is always buildable; work is developed in short-lived branches and
  merged through pull requests.
- Sensor transport, measurement parsing, Matter integration, and application
  policy remain separate modules.
- Wi-Fi passwords, Matter setup credentials, and generated local build files
  never enter Git.
- A feature is not complete without appropriate automated and hardware tests.

## Documentation

- [Architecture](docs/architecture.md)
- [Building the Matter-over-Wi-Fi baseline](docs/building.md)
- [Hardware](docs/hardware.md)
- [Development workflow](docs/development-workflow.md)
- [Continuous integration](docs/ci.md)
- [Roadmap](docs/roadmap.md)
