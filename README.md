# Overview
Alexa Lambda Linux (ALL) is a HW/SW reference design meant to enable quick prototyping and realization of the control and monitoring of things using Amazon Alexa Voice Services. The reference design has been used to control and monitor a home security system as a proof of concept.

Feature | Benefit
------------ | -------------
Integrated with Lambda and ASK | Quick bring-up of new voice controls
Real-time Linux / user space app dev model | Low effort to control fast real-world events
Raspberry PI, open source, AWS services | Low cost and quick deployment
End to end SSL/TLS integration | Customer data security

# Motivation and Goals of the project
TBA

# Requirements and System Architecture
![all](https://cloud.githubusercontent.com/assets/12125472/11692383/0e4a623e-9e54-11e5-8a78-b6fdf3eb9ba2.png)

# Using the Alexa Skills Kit (ASK) to create new skills
TBA

# Using AWS Lambda with ASK
TBA

# Interfacing Lambda with a Raspberry Pi to create an Alexa control point
TBA

# Safely interfacing the Raspberry Pi to real-world signals
TBA

# Leveraging real-time Linux on the Pi to ease interface constraints and development effort
TBA

# Security considerations
TBA

# Development and test environment
TBA

# Bill of materials and service cost considerations
TBA

# Appendix

## Prototype Output to Terminal
![screenshot from 2015-12-09 19 46 40](https://cloud.githubusercontent.com/assets/12125472/11706514/3819320c-9eae-11e5-95d0-af8bdee6ed24.png)
Note connections to AWS Lambda triggered by Alexa

## Prototype Current State
![proto pic1](https://cloud.githubusercontent.com/assets/12125472/11706674/a8721fcc-9eaf-11e5-8707-f780ae4ef86a.png)
![proto pic2](https://cloud.githubusercontent.com/assets/12125472/11706679/b0fb6950-9eaf-11e5-95be-3d668412c5e2.png)
Planning to move interface circuits from breadboard to board that fits in Raspberry PI housing 

## Keybus to GPIO I/F Unit Schematic
![netbus-gpio-if2](https://cloud.githubusercontent.com/assets/12125472/11706723/416ad890-9eb0-11e5-976f-e48f492587b6.png)
![netbus-gpio-if1](https://cloud.githubusercontent.com/assets/12125472/11706728/47b519f4-9eb0-11e5-8f12-56c11d6d14d0.png)

## PDFs of Block Diagram and System Overview
[all blockdia.pdf](https://github.com/goruck/all/files/57052/all.blockdia.pdf)
[all overview.pdf](https://github.com/goruck/all/files/57059/all.overview.pdf)
