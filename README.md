# Overview
Using voice to interface with devices and services around the home enables a rich and intuitive experience as shown by Amazon's huge successes with FireTV and Echo. However, with the exception of a few 3rd party point solutions such as Wemo, the voice user interface is generally not available around the home. One of the reasons for this is the difficulty and unfamiliarity of the technologies required to enable voice control. 

Alexa Lambda Linux (ALL) was developed to help accelerate this learning curve. ALL is a HW/SW reference design meant to enable quick prototyping and realization of the control and monitoring of things using Amazon Alexa Voice Services. A voice-controlled home security system has been built from the reference design as a proof of concept. 

This Read-me describes the system, the main components, its design, and implementation. I hope that people find it useful in creating voice interfaces of their own using Alex, Lambda, Linux, and the Raspberry Pi.

Feature | Benefit
------------ | -------------
Integrated with Lambda and ASK | Quick bring-up of new voice controls
Real-time Linux / user space app dev model | Low effort to control fast real-world events
Raspberry PI, open source, AWS services | Low cost and quick deployment
End to end SSL/TLS integration | Customer data security

# Requirements and System Architecture
I came up with the following high-level requirements that the project had to meet.

* Low cost.
* Extensible / reusable with low effort.
* Secure.
* Enable fast prototyping and development.
* Include at least one real world application. 

I felt that if the project met these requirements it would end up being useful to a very wide variety of voice applications. I also thought that I had to implement at one non-trivial real world application to prove that the design was robust and capable. Hence the last requirement. For that I choose to put a voice user interface on a commonly used home security system, the DSC Power832.

The system would have both cloud and (home-side) device components. I selected Amazon's Alexa as the cloud speech service and Amazon Web Services' Lambda to handle any cloud side processing required to interface between Alexa and the home-side devices. Alexa seemed to be an obvious choice to me (after all I work for Amazon :)) and it costs nothing to develop voice applications via the [Alexa Skills Kit] (https://developer.amazon.com/appsandservices) which certainly were needed to control random things around the home. [Lambda] (https://aws.amazon.com/lambda/) is ideal for quickly handling bursty processing loads, which is exactly what is needed to control things with voice. It also has a free tier if you stay under a certain amount of processing and above that its still very inexpensive. So, Alexa and Lambda seemed to be reasonable cloud choices given my requirements.

I selected the [Raspberry Pi 2] (https://www.raspberrypi.org/blog/raspberry-pi-2-on-sale/) as the platform to develop the home-side device components. The platform has a powerful CPU, plenty RAM, a wide variety of physical interfaces, has support for many O/Ss, and is inexpensive. I thought about using an even lower cost platform like [Arduino] (https://www.arduino.cc/) but I felt that given its lower capabilities vis-a-vis the Raspberry PI that would limit the types of home-side applications I could develop on that alone. For example, I thought I would need to use GNU/Linux in this project for extensibility and rapid development reasons. Arduino isn't really meant for to run Linux but the Pi is. The downside of using the Pi plus a high-level OS like vanilla Linux is that you give up the ability to react to quickly changing events. On the other hand, the Arduino running bare metal code is a very capable real-time machine. To be as extensible as possible, I didn't want to rule out the possibility of developing a real-time voice controlled application and wanted to avoid complex device side architectures like using an Arduino to handle the fast events connected to a Pi to handle the complex events. That seemed to work against my requirements. So, I came up with the idea of using [real-time Linux] (https://rt.wiki.kernel.org/index.php/Main_Page) on the Pi which I thought best met my goals. But it does have the downside that I'm no longer using a standard kernel and real-time programming requires a bit more thought and care than standard application development in Linux userspace.

I chose to focus on using the Pi's GPIOs to interface to the things around the home I wanted to add voice control to. I felt that this would allow me the maximum flexibility and with using real-time Linux, I thought I could run that interface fast. Its turned out that I was able to do GPIO reads and writes with less than +/- 1 us jitter, which is pretty amazing given that vanilla Linux at best can do about +/- 20 ms <sup>[1](#myfootnote1)</sup>. Of course, all the other physical interfaces (SPI, I2C, etc.) on the Pi are accessible though the normal Linux methods.

So given my requirements and the analysis above, I arrived at a system architecture with the following components.

* An Alexa Intent Schema / Utterance database developed using ASK.
* An AWS Lambda function in Node.js to handle the intent triggers from Alexa and send it back responses from the home device. 
* A home client build on Raspberry PI running real-time Linux with an application developed in C running in userspace.
* A hardware interface unit that handled the translation of the electrical signals between the Pi and the security system.
* The DSC Power832 security system connected via its Keybus interface to the Pi's GPIOs via the interface unit.

These components and the interconnections between them are shown in the diagram below.

![all](https://cloud.githubusercontent.com/assets/12125472/11692383/0e4a623e-9e54-11e5-8a78-b6fdf3eb9ba2.png)

# Design and Implementation of the Components

## Alexa Intent Schema / Utterance database
TBA

## AWS Lambda function
TBA

## Home Client

### Real-time Linux
TBA

### Application
TBA

### Startup
TBA

## Keybus to GPIO Interface Unit
TBA

# Development and Test environment
TBA

# Appendix

## DSC Power832 Overview
TBA 

## Bill of materials and service cost considerations
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

<a name="myfootnote1">1</a>: Footnote content goes here
