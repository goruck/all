# Overview
Using voice to interface with devices and services around the home enables a rich and intuitive experience as shown by Amazon's huge successes with FireTV and Echo. However, with the exception of a few 3rd party point solutions such as Wemo, the voice user interface is generally not available around the home. One of the reasons for this is the difficulty and unfamiliarity of the technologies required to enable voice control. 

Alexa Lambda Linux (ALL) was developed to help accelerate this learning curve. ALL is a HW/SW reference design meant to enable quick prototyping and realization of the control and monitoring of things using Amazon Alexa Voice Services. A voice-controlled home security system has been built from the reference design as a proof of concept. 

This Readme describes the system, the main components, its design, and implementation. I hope that people find it useful in creating voice interfaces of thier own using Alex, Lambda, Linux, and the Raspberry Pi.

Feature | Benefit
------------ | -------------
Integrated with Lambda and ASK | Quick bring-up of new voice controls
Real-time Linux / user space app dev model | Low effort to control fast real-world events
Raspberry PI, open source, AWS services | Low cost and quick deployment
End to end SSL/TLS integration | Customer data security

# Requirements and System Architecture
![all](https://cloud.githubusercontent.com/assets/12125472/11692383/0e4a623e-9e54-11e5-8a78-b6fdf3eb9ba2.png)

# Using the Alexa Skills Kit (ASK) to create new skills
TBD

# Using AWS Lambda with ASK
TBA

# Interfacing Lambda with a Raspberry Pi to create an Alexa control point
TBD

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

## The Keybus
TBA

## Proof of Concept Output to Terminal
![screenshot from 2015-12-09 19 46 40](https://cloud.githubusercontent.com/assets/12125472/11706514/3819320c-9eae-11e5-95d0-af8bdee6ed24.png)
Note: connections to AWS Lambda triggered by Alexa

## Proof of Concept Current State
![proto pic1](https://cloud.githubusercontent.com/assets/12125472/11706674/a8721fcc-9eaf-11e5-8707-f780ae4ef86a.png)
![proto pic2](https://cloud.githubusercontent.com/assets/12125472/11706679/b0fb6950-9eaf-11e5-95be-3d668412c5e2.png)
Note: planning to move interface circuits from breadboard to board that fits in Raspberry PI housing 

## Keybus to GPIO I/F Unit Schematic
![netbus-gpio-if2](https://cloud.githubusercontent.com/assets/12125472/11706723/416ad890-9eb0-11e5-976f-e48f492587b6.png)
![netbus-gpio-if1](https://cloud.githubusercontent.com/assets/12125472/11706728/47b519f4-9eb0-11e5-8f12-56c11d6d14d0.png)
Note: bypass capacitors not shown. 

## PDFs of Block Diagram and System Overview
[all blockdia.pdf](https://github.com/goruck/all/files/57052/all.blockdia.pdf)
[all overview.pdf](https://github.com/goruck/all/files/57059/all.overview.pdf)
