# Overview
Using voice to interface with devices and services around the home enables a rich and intuitive experience as shown by Amazon's huge successes with FireTV and Echo. However, with the exception of a few 3rd party point solutions such as Wemo, the voice user interface is generally not available in the home. One reason for this is the difficulty and unfamiliarity of the technologies required to enable voice control. 

Alexa Lambda Linux (ALL) was developed to help accelerate this learning curve. ALL is a HW/SW reference design meant to enable quick prototyping and realization of the control and monitoring of things using Amazon Alexa Voice Services. A voice-controlled home security system has been built from the reference design as proof of concept. 

The readme below describes the system, the main components, its design, and implementation. It is expected that people will find it useful in creating voice user interfaces of their own using Alexa, Lambda, Linux, and the Raspberry Pi.

Feature | Benefit
------------ | -------------
Integrated with Lambda and ASK | Quick bring-up of new voice controls
Real-time Linux / user space app dev model | Low effort to control fast real-world events
Raspberry PI, open source, AWS services | Low cost and quick deployment
End to end SSL/TLS integration | Customer data security

# Table of Contents
1. [Requirements and System Architecture](https://github.com/goruck/all#requirements-and-system-architecture)
2. [Design and Implementation of the Components](https://github.com/goruck/all#design-and-implementation-of-the-components)
   1. [Alexa Intent Schema / Utterance database](https://github.com/goruck/all#alexa-intent-schema--utterance-database)
   2. [AWS Lambda function](https://github.com/goruck/all#aws-lambda-function)
   3. [Raspberry Pi Controller / Server](https://github.com/goruck/all#raspberry-pi-controller--server)
   4. [Keybus to GPIO Interface Unit](https://github.com/goruck/all#keybus-to-gpio-interface-unit)
3. [Development and Test environment](https://github.com/goruck/all#development-and-test-environment)
4. [Overall Hardware Design and Considerations](https://github.com/goruck/all#overall-hardware-design-and-considerations)
5. [Bill of Materials and Service Cost Considerations](https://github.com/goruck/all#bill-of-materials-and-service-cost-considerations)
6. [Appendix](https://github.com/goruck/all#appendix)

# Requirements and System Architecture
Please see below the high-level requirements that the project had to meet.

* Low cost
* Extensible / reusable with low effort
* Secure
* Enable fast prototyping and development
* Include at least one real world application

Meeting these requirements would prove this project useful to a very wide variety of voice interface applications. In addition, implementing a non-trivial "real-world" application would show that the design is robust and capable. Hence the last requirement, which drove the implementation of a voice user interface on a standard home security system, the DSC Power832.

The system needs both cloud and (home-side) device components. Amazon's Alexa was selected as the cloud speech service and Amazon Web Services' Lambda was selected to handle the cloud side processing required to interface between Alexa and the home-side devices. Alexa is a good choice given that its already integrated with Lambda and has a variety of voice endpoints including Echo and FireTV and it costs nothing to develop voice applications via the [Alexa Skills Kit](https://developer.amazon.com/public/solutions/alexa/alexa-skills-kit). [Lambda](https://aws.amazon.com/lambda/) is ideal for quickly handling bursty processing loads, which is exactly what is needed to control things with voice. It also has a free tier under a certain amount of processing and above that its still very inexpensive. So, Alexa and Lambda are reasonable cloud choices given the requirements above.

The [Raspberry Pi 2](https://www.raspberrypi.org/blog/raspberry-pi-2-on-sale/) was designated as the platform to develop the home-side device components. The platform has a powerful CPU, plenty RAM, a wide variety of physical interfaces, has support for many OSs, and is inexpensive. It is possible to use an even more inexpensive platform like [Arduino](https://www.arduino.cc/) but given its lower capabilities vis-a-vis the Raspberry PI this would limit the types of home-side applications that could be developed. For example, use of GNU/Linux is desirable in this project for extensibility and rapid development reasons. Arduino isn't really capable of running Linux but the Pi is. The downside of using the Pi plus a high-level OS like vanilla Linux is that the system cannot process quickly changing events (i.e., in "real-time"). On the other hand, the Arduino running bare metal code is a very capable real-time machine. To be as extensible as possible, the system needs to support the development of real-time voice controlled applications and without complex device side architectures like using an Arduino to handle the fast events connected to a Pi to handle the complex events. Such an architecture would be inconsistent with the project requirements. Therefore [real-time Linux](https://rt.wiki.kernel.org/index.php/Main_Page) was chosen as the OS on the Pi. However, this comes with the downsides of using a non-standard kernel and real-time programming is less straightforward than standard application development in Linux userspace.

In the reference design, the Pi's GPIOs are the primary physical interface to the devices around the home that are enabled with a voice UI. This allows maximum interface flexibility, and with using real-time Linux, the GPIO interface runs fast. The reference design enables GPIO reads and writes with less than 60 us latency, vanilla Linux at best can do about 20 ms. Of course, all the other physical interfaces (SPI, I2C, etc.) on the Pi are accessible in the reference design though the normal Linux methods.

The requirements and the analysis above drove a system architecture with the following components.

* An Alexa Intent Schema / Utterance database developed using ASK
* An AWS Lambda function in Node.js to handle the intent triggers from Alexa and send it back responses from the home device 
* A home device built on Raspberry PI running real-time Linux with a server application developed in C running in userspace
* A hardware interface unit that handled the translation of the electrical signals between the Pi and the security system
* The DSC Power832 security system connected via its Keybus interface to the Pi's GPIOs via the interface unit

These components and the interconnections between them are shown in the diagram below.

![all](https://cloud.githubusercontent.com/assets/12125472/11692383/0e4a623e-9e54-11e5-8a78-b6fdf3eb9ba2.png)

Note that although the development of the architecture described above appears very waterfall-ish, the reality is that it took many iterations of architecture / design / test to arrive at the final system solution.

# Design and Implementation of the Components

## Alexa Intent Schema / Utterance database
An Amazon applications developer account is required to get access to the [Alexa Skills Kit (ASK)](https://developer.amazon.com/public/solutions/alexa/alexa-skills-kit), one can be created one at https://developer.amazon.com/appsandservices. There's a [getting started guide](https://developer.amazon.com/appsandservices/solutions/alexa/alexa-skills-kit/getting-started-guide) on the ASK site on how to create a new skill. The skill developed to control the alarm panel, named *panel*, used the example skill *color* as a starting point. Amazon makes the creation of a skill relatively easy but careful thinking through the voice interaction is required. The *panel* skill uses a mental model of attaching a voice command to every button on the alarm's keypad and an extra command to give the status of the system. The alarm system status is the state of the lights on the keypad (e.g., armed, bypass, etc). Four-digit code input is also supported, which is useful for a PIN. The Amazon skill development tool takes you through the following steps in creating the skill:

1. Skill Information - Invocation Name, Version, and Service Endpoint (the Lambda ARN in this project)
2. Interaction Model - Intent Schema, Custom Slot Types, and Sample Utterances
3. Test
4. Description
5. Publishing Information (the *panel* skill is not currently published)

The Intent Schema for the *panel* skill is as follows:

```javascript
{
  "intents": [
    {
      "intent": "MyNumIsIntent",
      "slots": [
        {
          "name": "Keys",
          "type": "LIST_OF_KEYS"
        }
      ]
    },
    {
      "intent": "MyCodeIsIntent",
      "slots": [
        {
          "name": "Code",
          "type": "AMAZON.FOUR_DIGIT_NUMBER"
        }
      ]
    },
    {
      "intent": "WhatsMyStatusIntent"
    },
    {
      "intent": "AMAZON.HelpIntent"
    }
  ]
}
```

Where "LIST_OF_KEYS" is a custom slot type defined as:

```javascript
0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | star | pound | stay | away
```

The sample utterances for the skill are:

1. WhatsMyStatusIntent panel status
2. WhatsMyStatusIntent status
3. WhatsMyStatusIntent its status
4. WhatsMyStatusIntent what is panel status
5. WhatsMyStatusIntent what is the panel status
6. WhatsMyStatusIntent what is the status
7. MyNumIsIntent {Keys}
8. MyCodeIsIntent {Code}

The "LIST_OF_KEYS" slot enables the intent MyNumIsIntent to activate when Alexa hears the name of the buttons on the keypad defined by the slot. The intent "WhatsMyStatusIntent" activates when Alexa hears any of the status related utterances listed above. When the intents activate, they cause the service endpoint Lambda function to run and perform specific processing depending on the user intent.

The skill is invoked by saying to Alexa "ask panel" or "tell panel".

## AWS Lambda function
An Amazon Web Services account is needed to use Lambda, one can be created at at https://aws.amazon.com/. Like all AWS products, there is good [documentation](https://aws.amazon.com/lambda/) available on the AWS site that should be read to come up to speed, if not already familiar. Still it can be challenging to use the product, if unfamiliar with javascript, Node, and asynchronous programming which are all essential to using Lambda. Several books and articles including [*Sams Teach Yourself Node.js in 24 Hours*](http://smile.amazon.com/dp/0672335956) by Ornbo, [*JavaScript: The Good Parts*](http://smile.amazon.com/dp/0596517742) by Crockford, and [*Asynchronous programming and continuation-passing style in JavaScript*](http://www.2ality.com/2012/06/continuation-passing-style.html) by Rauschmayer can help to accelerate the learning curve. Lambda and Node.js is very well-suited to scale cloud side processing associated with applications like voice control, given that Lambda was designed to handle bursty applications and the non-blocking I/O benefits of Node.js.

The *color* skill and Lambda function example provided by ASK was used as a template to create the Lambda function for *panel*. Since the Lambda function needed to talk to a remote Raspberry Pi server, that functionality was added as well as modifying the logic and speech responses to suit the alarm application. One of the biggest challenges in this development is the placement of the  callbacks that returned responses back to Alexa due to the async nature of Node.js. The rest of the Lambda function development was straightforward. 

Below are a few key parts of the code which is listed in its entirety [elsewhere](https://github.com/goruck/all/blob/master/lambda/all.js).

The code below sets up the ability to use the tls method to open, read, and write a TCP socket that connects to the remote Pi server. the tls method provides both authentication and encryption between the Lambda client and the Pi server. 

```javascript
var fs = require('fs');
var tls = require('tls');
var PORT = XX;
var HOST = 'XXX.XXX.XXX.XXX'; // todo: use FQDN instead of IP
    
var options = {
  host: HOST,
  port: PORT,
  rejectUnauthorized: true,
  cert: fs.readFileSync('client.crt'),
  key: fs.readFileSync('client.key'),
  ca: fs.readFileSync('ca.crt')
};
```

The code below writes a value to the remote Pi server that gets translated into an alarm keypad command. 

```javascript
var socket = tls.connect(options, function() {
  console.log('connected to host ' +HOST);
  if(socket.authorized){
    console.log('host is authorized');
  } else {
    console.log('host cert auth error: ', socket.authorizationError);
  }
  socket.write(num +'\n');
  console.log('wrote ' +num);
  socket.end;
  console.log('disconnected from host ' +HOST);
  callback(sessionAttributes,
    buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
});
```

The code below read alarm status coming back from the Pi server and sends it back to Alexa. 

```javascript
socket.on('data', function(data) {
   read += data.toString();
});
       
socket.on('end', function() {
   socket.end;
   console.log('disconnected from host ' +HOST);
   console.log('host data read: ' +read);
   speechOutput = read;
   callback(sessionAttributes,
      buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
});
```

## Raspberry Pi Controller / Server

### Real-time Linux
The Linux disto on the Pi is based on Debian 7.0 "Wheezy" with a fork of the [Raspberry Pi Linux kernel](https://github.com/raspberrypi/linux) patched with rt-patch and configured as a fully preemptible kernel. The patched and configured source code for the Raspberry Pi real-time kernel is available on [GitHub](https://github.com/emlid/linux-rt-rpi). The wiki [real-time Linux](https://rt.wiki.kernel.org/index.php/Main_Page) is an invaluable source of information on the rt-patch and how to write real-time code.

The patched version of the kernel includes the following changes.
* Replaced default kernel with PREEMPT_RT kernel 3.18.9-rt5-v7+
* Enabled non-FIQ USB driver (currently FIQ driver is not compatible with RT-patch on RPi2)
* Enabled camera, SPI, I2C and set its speed to 1MHz
* Disabled serial console
* Changed default WiFi network parameters

The latency of the patched kernel was measured using [Cyclictest](https://rt.wiki.kernel.org/index.php/Cyclictest) which showed a worse case of less than 60 us. So, that seemed to indicate that applications could respond to events at up to about 15 kHz. The typical latency of a vanilla 3.18 kernel on the Pi was measured at 20 ms.

The distribution of the latency on the patched kernel as measure by Cyclictest is shown below.

![pi latency](https://cloud.githubusercontent.com/assets/12125472/11770022/4d3df3fe-a1a9-11e5-91b7-281b4b064da6.gif)

### Controller / Server Application
The application code on the Pi emulates a DSC keypad controller running in the Linux system's userspace. The application code in its entirety can be found [here](https://github.com/goruck/all/blob/master/rpi/kprw-server.c), this section captures important design considerations that are not obvious from the code itself or the comments therein.

The DSC system is closed and proprietary and besides the installation and programming information in the DSC manuals and what hackers have managed to figured out and posted on the Internet and GitHub, there isn't much information available about the protocol it uses to communicate with its keypads. Also, much of of what is on the Internet relies on using an IP interface bridge like this [one](http://www.eyezon.com/?page_id=176) from Eyezon. A bridge like this instead of the code on the Raspberry Pi could have been used in this project but that would have made the reference design much less generic (and much less fun to develop). A lot of useful information on how to hack the DSC system can be found in the [Arduino-Keybus](https://github.com/emcniece/Arduino-Keybus) GitHub repo, but this project uses an Arduino which was ruled out (see above) and it was read-only. Still, it is very educational. Although these sources helped, a lot of reverse engineering was required to figure out the protocol used on the DSC Keybus. The reverse engineering mainly consisted of using an early version of the application code to examine the bits sent from the panel to the keypads and the keypads to the panel in response to keypad button presses and other events, like motion and door / window openings and closings. It was determined that the first two bytes of each message indicates its type, followed by a variable number of data bits including error protection. For example a message with its first two bytes equal to 0x05 is telling the keypad to illuminate specific status LEDs, such as BYPASS, READY, etc and a message type equal to 0xFF indicates keypad to panel data is being sent. See the *decode()* function in the application code for details regarding the various message types and their contents.

The application code consists of three main parts:

* A thread called *panel_io()* that handles the bit-level processing needed to assemble messages from the keybus serial data
* A thread called *msg_io()* that handles the message-level output processing
* A server called *panserv()* running in the main thread that communicates with an external client for commands and status

The application code directly accesses the GPIO's registers for the fastest possible reads and writes. The direct access code is based on [this](http://elinux.org/RPi_GPIO_Code_Samples#Direct_register_access) information from the Embedded Linux Wiki at elinux.org. The function *setup_io()* in the application code sets up a memory regions to access the GPIOs. The information in the Broadcom BCM2835 ARM Peripherals document is very useful to understand how to safely access the Pi's processor peripherals. It is [here](http://elinux.org/RPi_Documentation) on the Embedded Linux Wiki site.

Inter-thread communication is handled safely via read and write FIFOs without any synchronization (e.g., using a mutext). Its not desirable to use conventional thread synchronization methods since they would block the *panel_io()* thread. There is no way (to my knowledge) to tell the panel not to send data on the keybus, so if *panel_io()* was blocked by another thread accessing a shared FIFO, the application code would drop messages. A good solution to this problem was found in the article [Creating a Thread Safe Producer Consumer Queue in C++ Without Using Locks](http://blogs.msmvps.com/vandooren) by Vandooren, which is what was implemented with a few modifications.

Running code under real-time Linux requires a few special considerations. Again, the [real-time Linux](https://rt.wiki.kernel.org/index.php/Main_Page) wiki was used extensively to guide the efforts in this regard. The excellent book [The Linux Programming Interface](http://man7.org/tlpi/) by Kerrisk also was very helpful to understand more deeply how the Linux kernel schedules tasks. There are three things needed to be set by a task in order to provide deterministic real time behavior:

1. Setting a real time scheduling policy and priority by use of the *sched_setscheduler()* system call and its pthread cousins. Note that *panel_io()*, *message_io()*, and *main()* were all given the same scheduling policy (SCHED_FIFO) but *panel_io()* has a higher priority than *message_io()* and *main()*. This was done to ensure that the bit-level processing would have enough time to complete. 
2. Locking memory so that page faults caused by virtual memory will not undermine deterministic behavior. This is done with the *mlockall()* system call. 
3. Pre-faulting the stack, so that a future stack fault will not undermine deterministic behavior. This is done by simply accessing each element of the program's stack by use of the *memset()* system call. 

Extensive use of the *clock_nanosleep()* function from librt is used in the code. For example, its use is shown in the *panel_io()* thread where it awakens the thread every INTERVAL (10 us) to read and write to the GPIOs driving the keybus clock and data lines (via the interface unit). The thread contains logic to detect the start of word marker, to detect high and low clock periods, and to control precisely when the GPIOs are read and written. The below taken from *panel_io()* causes the thread to wait for valid data before reading the GPIO.

```c
t.tv_nsec += KSAMPLE_OFFSET;
tnorm(&t);
clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL); // wait KSAMPLE_OFFSET for valid data
wordkr_temp = (GET_GPIO(PI_DATA_IN) == PI_DATA_HI) ? '0' : '1'; // invert
```

Even though the *panel_io()* is given higher priority than the other two threads, other critical threads in the Linux kernel must have higher still priority for the overall system to function. Therefore it is possible that a higher priority kernel task could preempt *panel_io()* and cause it to drop bits. The real-time Linux wiki suggests this solution:

*"In general, I/O is dangerous to keep in an RT code path. This is due to the nature of most filesystems and the fact that I/O devices will have to abide to the laws of physics (mechanical movement, voltage adjustments, <whatever an I/O device does to retrieve the magic bits from cold storage>). For this reason, if you have to access I/O in an RT-application, make sure to wrap it securely in a dedicated thread running on a disjoint CPU from the RT-application."* 

Based on this, the *panel_io()* thread was locked to its own CPU and the other threads are locked to the other three CPUs (its really great to have four CPUs at one's disposal). The application code sets the thread CPU affinity as shown below. 

```c
// CPU(s) for main and message i/o threads
  CPU_ZERO(&cpuset_mio);
  CPU_SET(1, &cpuset_mio);
  CPU_SET(2, &cpuset_mio);
  CPU_SET(3, &cpuset_mio);
  CPU_ZERO(&cpuset_main);
  CPU_SET(1, &cpuset_main);
  CPU_SET(2, &cpuset_main);
  CPU_SET(3, &cpuset_main);
  // CPU(s) for panel i/o thread
  CPU_ZERO(&cpuset_pio);
  CPU_SET(0, &cpuset_pio);
```

With the threads pinning to the CPUs in this way and with the priority of *panel_io()* being greater than the other two threads in the application code (but not greater than the kernel's critical threads), the system exhibits robust real-time performance.

The server uses openSSL for authentication and encryption. For development and test purposes, self-signed certificates and the IP address of the server is used instead of a Fully Qualified Domain Name. The cert and private key are generated with the following commands.

```bash
$ openssl genrsa -out privateKey.key 2048
$ openssl req -new -key privateKey.key -out privateKey.csr
$ openssl x509 -req -in privateKey.csr -signkey privateKey.key -out certificate.crt -days 500 -extfile key.ext
```
Where key.ext contains "subjectAltName = IP:xxx.xxx.xxx.xxx" (replace with the IP address of your server). 

Production code should use certificates signed by a real CA and a FQDN for the server, registered with a DNS. The server also uses TCP Wrapper daemon for secure access. TCP Wrapper uses the *hosts_ctl()* system call from libwrap library to limit client access via the rules defined in /etc/host.deny and /etc/host.allow files. The rules are set so that only clients with local IP addresses and AWS IP addresses are allowed access to the server.

The application code needs to be compiled with the relevant libraries and executed with su privileges, per the following.

```bash
$ gcc -Wall -o kprw-server kprw-server.c -lrt -lpthread -lwrap -lssl -lcrypto
$ sudo ./kprw-server
```

### Startup
The Raspberry Pi is used here as an embedded system so it needs to come up automatically after power on, including after a possible loss of power. The application code defines the GPIOs as follows:

```c
// GPIO pin mapping.
#define PI_CLOCK_IN	(13) // BRCM GPIO13 / PI J8 Pin 33
#define PI_DATA_IN	(5)  // BRCM GPIO05 / PI J8 Pin 29
#define PI_DATA_OUT	(16) // BRCM GPIO16 / PI J8 Pin 36
``` 

These GPIOs need to be in a safe state after power on and boot up. Per the Broadcom BCM2835 ARM Peripherals document, these GPIOs are configured as inputs at reset and the kernel doesn't change that during boot, so they won't cause any the keybus serial data line to be pulled down before the application code initializes them. The BRCM GPIO16 was selected for the active high signal that drives the keybus data line since that GPIO has the option of being driven low from an external pull down. Other GPIOs have optional pull-ups. Even though the pull resistors are disabled by default, this removes the possibility of the keybus data line becoming active if software somehow enabled the pull resistor by mistake. 

The code below was added to /etc/rc.local so that the application code would automatically run after powering on the Pi.

```bash
/home/pi/all/rpi/kprw-server > /dev/null &
```

At some point provisions will be added to automatically restart the application code in the event of a crash.

In testing it was observed that the Pi sometimes disconnected from the wireless network and did not automatically reconnect, especially when it was connected to a monitor via HDMI (it may have a desense problem). In the event the Pi's wifi does not automatically reconnect to the network after it loses the connection, a special script is run by cron. The script checks for network connectivity every 5 minutes and if the network is down, the wireless interface is automatically restarted. The script and cron entry are shown below are are based off of [this](http://alexba.in/blog/2015/01/14/automatically-reconnecting-wifi-on-a-raspberrypi/) technical blog. 

/usr/local/bin/wifi_rebooter.sh:
```bash
#!/bin/bash

# IP for the server you wish to ping (8.8.8.8 is a public Google DNS server)
SERVER=8.8.8.8

# Only send two pings, sending output to /dev/null
ping -c2 ${SERVER} > /dev/null

# If the return code from ping ($?) is not 0 (meaning there was an error)
if [ $? != 0 ]
then
    # Restart the wireless interface
    ifdown --force wlan0
    ifup wlan0
fi
```
Entry in root's crontab:
```bash
*/5 * * * *   root    /usr/local/bin/wifi_rebooter.sh
```

## Keybus to GPIO Interface Unit

### Keybus Electrical and Timing Characteristics
The keybus is a DSC proprietary serial bus that runs from the panel to the sensors and controller keypads in the house. This bus needs to understood from an electrical, timing, and protocol point of view (see above) before the Pi can be interfaced to the panel. There isn't a lot of information on the keybus other than what's in the DSC installation manual and miscellaneous info on the Internet posted by hackers. Essentially, this is a two wire bus with clock and data, plus ground and the supply from the panel. The supply is 13.8 V (nominal) and the clock and data transition between 0 and 13.8 V. (The large voltage swings make sense given that it provides good noise immunity against interference picked up from long runs of the bus through the house.) The DSC manual states the bus supply can source a max of 550 mA. The sum of the current from all the devices presently on the bus was about 400 mA. This meant that the interface unit could draw no more than 150 mA.

An oscilloscope was used to reverse engineer the protocol. Some screen-shots and analysis from them are below.

![whole-word-ann](https://cloud.githubusercontent.com/assets/12125472/11801916/17eb6cf8-a29f-11e5-8d3c-5cd4d39ac7ed.gif)

This shows an entire word with the start of new word marker, which is the clock being held high for a relatively long time. The clock rate when active is 1 KHz. The messages between the panel and keypads are of variable length. The typical message from the panel to the keypads is 43 bits (43 ms in duration) and the start of the new word clock marker is about 15 ms. Thus, a typical message time including start of new marker is about 58 ms. Messages can be up to 62 bits long but they are not frequent. 

![data-clock-close](https://cloud.githubusercontent.com/assets/12125472/11801938/55c9817c-a29f-11e5-82a7-1eac18a5abc4.gif)

This shows a closer view of the clock and data, with data being sent between the panel and keypad and vice-versa. Data from the panel to the keypads transitions on the rising edge of the clock and becomes valid about 120 us later. Data from the keypads to the panel transitions at falling edge of the clock is likely registered in the panel on the next rising edge of the clock. The keypad data needs to be held for at least 25 us after the rising edge of the clock for it to be properly registered. Timers in the Pi's application code must adhere to these values for reliable data transfers to happen. The spikes at 2.5 and 4.5 ms are most likely due to the pull-up resistor on the data line in the panel causing it to be pulled high before the driver on the keypad has a chance to pull it low, which happens when the keypad sends a 0 bit to the panel.

From this data, it is clear that the panel's data line is bidirectional (with direction switched by clock level) and pulled up internally when configured as an input. The strength of the pull-up resister needs to be known in order to design the interface circuit correctly and via experimentation it was determined to be approximately 5 KΩ.

### Raspberry Pi GPIO Electrical Characteristics
Data on the electrical characteristics of the Pi's GPIOs is also required to design the interface. There is not really the right level of detail available from the Raspberry Pi standard information. But there is excellent information on the Mosaic Industries' website [here](http://www.mosaic-industries.com/embedded-systems/microcontroller-projects/raspberry-pi/gpio-pin-electrical-specifications). This information is super helpful and detailed. Some of the key points from the article are below. 

* These are 3.3 volt logic pins. A voltage near 3.3 V is interpreted as a logic one while a voltage near zero volts is a logic zero. A GPIO pin should never be connected to a voltage source greater than 3.3V or less than 0V, as prompt damage to the chip may occur as the input pin substrate diodes  conduct. There may be times when you may need to connect them to out ­of ­range voltages – in those cases the input pin current must be limited by an external resistor to a value that prevents harm to the chip. I recommend that you never source or sink more than 0.5 mA into an input pin
* To prevent excessive power dissipation in the chip, you should not source/sink more current from the pin than its programmed limit. So, if you have set the current capability to 2 mA, do not draw more than 2 mA from the pin
* Never demand that any output pin source or sink more than 16 mA
* Current sourced by the outputs is drawn from the 3.3 V supply, which can supply only 50 mA maximum. Consequently, the maximum you can source from all the GPIO outputs simultaneously is less than 50 mA. You may be able to draw transient currents beyond that limit as they are drawn from the bypass capacitors on the 3.3 V rail, but don't push the envelope! There isn't a similar limitation on sink current. You can sink up to 16 mA each into any number of GPIO pins simultaneously. However, transient sink currents do make demands on the board's bypass capacitors, so you may get into trouble if all outputs switch their maximum current synchronously
* Do not drive capacitive loads. Do not place a capacitive load directly across the pin. Limit current into any capacitive load to a maximum transient current of 16 mA. For example, if you use a low pass filter on an output pin, you must provide a series resistance of at least 3.3V/16mA = 200 Ω

### Interface Circuit Design
Its desirable to electrically isolate the panel and Pi to prevent ground loops and optoisolators are an obvious choice. But they do consume a fair amount of current and the keybus data and clock lines cannot supply enough to directly drive them. Therefore, the keybus clock and data lines have buffers (CD4010) between them and the optoisolators. Out of an abundance of caution, buffers (74HC126) were also added between the clock and data optoisolators and the Pi's GPIOs, but they are not really needed since the Pi's GPIOs come out of reset being able to supply or sink 8 mA while the interface circuit only needs a 2 mA supply. The '126 buffers were removed in the final design. A high Current Transfer Ratio (CTR) optoisolator configured to drive a FET with logic-level gate voltage threshold was used to pull down the data line with sufficient strength. The other optoisolators are not high CTR (high CTR = $$). The interface schematic is shown below.

![interface](https://cloud.githubusercontent.com/assets/12125472/11998220/3d1befd6-aa4a-11e5-83bf-9cc1cec3bb72.png)

# Development and Test environment
The Raspberry Pi was used headless throughout development and so ssh was used from another Linux machine to edit and debug the application code with gedit. The application code was compiled directly on the Raspberry Pi, given its small size it compiled quickly which obviated the need to setup a cross-compiler on a more capable machine.

Clients written in Node.js and C were used to test the server before integration with Lambda. This code is is in the goruck/all/test repo. 

The ASK and Lambda test tools available in the SDK and Lambda status information from AWS Cloudwatch Logs were used extensively during development and test.

Schematic entry was done using gEDA's Schematic Editor.

# Bill of materials and service cost considerations
The hardware bill of materials total $85 and are as follows.

Item | Qty | Cost
-----|-----|-----
Raspberry Pi 2 Model B Quad-Core 900 MHz 1 GB RAM | 1 | $38
Various ICs and passives | 15 | $15
HighPi Raspberry Pi B+/2 Case | 1 | $14
32GB microSDHC | 1 | $12
JBtek® DIY Raspberry Pi 2 Model B Hat | 1 | $6
Totals | 19 | $85

The Lambda service cost will vary according to how often the Alexa skill is invoked and how much compute time the function consumes per month. A typical Lambda session for the alarm prototype lasted less than 450 ms with AWS typically billing me for 500 ms (they round up to the nearest 100 ms) and was allocated 128 MB memory. The free tier provides provides the first 400,000 GB-s processing and the first one million requests per month at no cost. At 128 MB the Alexa panel skill can use 3,200,000 seconds of free processing per month (6.2 million invocations of the skill) and the first 1 million invocations will be at **no cost**. AWS is truly built for scale. This assumes less than 1 GB per month external data is transfered to/from the Lambda function so that the Free tier is maintained.

See the [AWS website](https://aws.amazon.com/lambda/pricing/) for complete pricing information.

# Overall Hardware Design and Considerations
The interface circuit was first breadboarded and then moved to a prototyping board that fits in an extended Raspberry Pi enclosure. The prototyping board is from [JBtek®](http://smile.amazon.com/dp/B00WPFF9OA) and the enclosure is from [MODMYPI](http://www.modmypi.com/raspberry-pi/cases/highpi/highpi-raspberry-pi-b-plus2-case). The extended Pi enclosure also offers more height for components. However, there isn't a lot of area to fit the components; the interface circuit fit nicely but it would get increasingly difficult to fit additional components if required. There is height in the enclosure to stack another prototyping board but the component heights would have to be kept to a minimum. If there's a need to move beyond the circuit complexity required by the interface then a custom designed PCB would probably be the best route. Photographs of the current prototype are shown below.

![img_0619](https://cloud.githubusercontent.com/assets/12125472/12216367/b6de26a4-b693-11e5-8ca5-6b1e528edb9f.JPG)

![img_0620](https://cloud.githubusercontent.com/assets/12125472/12216370/bc4a32cc-b693-11e5-9e1c-312df70cbe27.JPG)

![img_0623](https://cloud.githubusercontent.com/assets/12125472/12216371/c0dbaa28-b693-11e5-8726-cf3ce1a0132b.JPG)

![img_0625](https://cloud.githubusercontent.com/assets/12125472/12216373/c5025aac-b693-11e5-8cd4-05d1c2cce285.JPG)

Note: most components were salvaged from other projects and so the selection and placement is not optimized for either cost or size. 

# Appendix

## DSC Power832 Overview
Technical manuals and general overview information about the Power832 can be found on [DSC's](http://www.dsc.com/index.php?n=library&o=view_documents&id=1) website.

## Proof of Concept Output to Terminal
![screenshot from 2015-12-09 19 46 40](https://cloud.githubusercontent.com/assets/12125472/11706514/3819320c-9eae-11e5-95d0-af8bdee6ed24.png)
Note: connections to AWS Lambda triggered by Alexa

## Early Proof of Concept Photo
![proto pic1](https://cloud.githubusercontent.com/assets/12125472/11706674/a8721fcc-9eaf-11e5-8707-f780ae4ef86a.png)
![proto pic2](https://cloud.githubusercontent.com/assets/12125472/11706679/b0fb6950-9eaf-11e5-95be-3d668412c5e2.png)

## Original schematics with design notes
![keybus-gpio-if1](https://cloud.githubusercontent.com/assets/12125472/11919445/f16955be-a707-11e5-8c5d-1de31212bf9a.png)
![keybus-gpio-if2](https://cloud.githubusercontent.com/assets/12125472/11919447/f453d4d4-a707-11e5-988a-f284c41c4085.png)
