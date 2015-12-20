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

The system would have both cloud and (home-side) device components. I selected Amazon's Alexa as the cloud speech service and Amazon Web Services' Lambda to handle any cloud side processing required to interface between Alexa and the home-side devices. Alexa seemed to be a good choice to me (its already integrated with Lambda and has a variety of voice endpoints including Echo and FireTV) and it costs nothing to develop voice applications via the [Alexa Skills Kit](https://developer.amazon.com/public/solutions/alexa/alexa-skills-kit) which certainly were needed to control random things around the home. [Lambda](https://aws.amazon.com/lambda/) is ideal for quickly handling bursty processing loads, which is exactly what is needed to control things with voice. It also has a free tier if you stay under a certain amount of processing and above that its still very inexpensive. So, Alexa and Lambda seemed to be reasonable cloud choices given my requirements.

I selected the [Raspberry Pi 2](https://www.raspberrypi.org/blog/raspberry-pi-2-on-sale/) as the platform to develop the home-side device components. The platform has a powerful CPU, plenty RAM, a wide variety of physical interfaces, has support for many O/Ss, and is inexpensive. I thought about using an even lower cost platform like [Arduino](https://www.arduino.cc/) but I felt that given its lower capabilities vis-a-vis the Raspberry PI that would limit the types of home-side applications I could develop on that alone. For example, I thought I would need to use GNU/Linux in this project for extensibility and rapid development reasons. Arduino isn't really meant for to run Linux but the Pi is. The downside of using the Pi plus a high-level OS like vanilla Linux is that you give up the ability to react to quickly changing events. On the other hand, the Arduino running bare metal code is a very capable real-time machine. To be as extensible as possible, I did not want to rule out the possibility of developing a real-time voice controlled application and wanted to avoid complex device side architectures like using an Arduino to handle the fast events connected to a Pi to handle the complex events. That seemed to work against my requirements. So, I came up with the idea of using [real-time Linux](https://rt.wiki.kernel.org/index.php/Main_Page) on the Pi which I thought best met my goals. But it does have the downside that I'm no longer using a standard kernel and real-time programming requires a bit more thought and care than standard application development in Linux userspace.

I chose to focus on using the Pi's GPIOs to interface to the things around the home I wanted to add voice control to. I felt that this would allow me the maximum flexibility and with using real-time Linux, I thought I could run that interface fast. Its turned out that I was able to do GPIO reads and writes with less than 60 us latency, which is pretty amazing given that vanilla Linux at best can do about 20 ms <sup>[1](#myfootnote1)</sup>. Of course, all the other physical interfaces (SPI, I2C, etc.) on the Pi are accessible though the normal Linux methods.

So given my requirements and the analysis above, I arrived at a system architecture with the following components<sup>[2](#myfootnote2)</sup>.

* An Alexa Intent Schema / Utterance database developed using ASK.
* An AWS Lambda function in Node.js to handle the intent triggers from Alexa and send it back responses from the home device. 
* A home device built on Raspberry PI running real-time Linux with a server application developed in C running in userspace.
* A hardware interface unit that handled the translation of the electrical signals between the Pi and the security system.
* The DSC Power832 security system connected via its Keybus interface to the Pi's GPIOs via the interface unit.

These components and the interconnections between them are shown in the diagram below.

![all](https://cloud.githubusercontent.com/assets/12125472/11692383/0e4a623e-9e54-11e5-8a78-b6fdf3eb9ba2.png)

# Design and Implementation of the Components

## Alexa Intent Schema / Utterance database
You need to have an Amazon applications developer account to get access to the [Alexa Skills Kit (ASK)](https://developer.amazon.com/public/solutions/alexa/alexa-skills-kit), so I first created one at https://developer.amazon.com/appsandservices to get going. There's a [getting started guide](https://developer.amazon.com/appsandservices/solutions/alexa/alexa-skills-kit/getting-started-guide) on the ASK site on how to create a new skill. I used that and the example skill *color* to create the skill used to control the alarm panel which I named *panel*. Amazon makes the creation of the skill pretty easy but you do have to think through the voice interaction a bit. I used the mental model of attaching a voice command to every button on the alarm's keypad and an extra command to give the status of the system which is simply the state of the lights on the keypad (e.g., armed, bypass, etc.) to help me create the skill. The Amazon kill dev tool takes you through the following steps in creating the skill:

1. Skill Information - Invocation Name, Version, and Service Endpoint (the Lambda ARN in my case).
2. Interaction Model - Intent Schema, Custom Slot Types, and Sample Utterances.
3. Test.
4. Description.
5. Publishing Information.

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

The "LIST_OF_KEYS" slot enables the intent MyNumIsIntent to activate when Alex hears the name of the buttons on the keypad defined by the slot. The intent "WhatsMyStatusIntent" activates when Alexa hears any of the status related utterances listed above <sup>[3](#myfootnote3)</sup>. When the intents activate, they cause the service end point Lambda funcation to run and perform specific processing depending on the user intent. 

## AWS Lambda function
You need to have an Aamazon Web Services account to use Lambda. I already was an EC2 user, so I didn't have to set one up but if you can do so at https://aws.amazon.com/. Like all AWS products, there is good [documentation](https://aws.amazon.com/lambda/) available on the AWS site that you should read to come up to speed on it, if not already familiar. I did that but I still found it challenging to use the product, mainly due to my unfamiliarity with javascript, Node, and asynchronous programming which are all essential to using Lambda. To get up to speed, I read a few books and articles including [*Sams Teach Yourself Node.js in 24 Hours*](http://smile.amazon.com/dp/0672335956) by Ornbo, [*JavaScript: The Good Parts*](http://smile.amazon.com/dp/0596517742) by Crockford, and [*Asynchronous programming and continuation-passing style in JavaScript*](http://www.2ality.com/2012/06/continuation-passing-style.html) by Rauschmayer. Over time, I understood how well suited Lambda and Node.js is to helping scale the cloud side to handle applications like voice control of things given how Lambda was designed to handle bursty applications and the non-blocking I/O benefits of Node.js.

I used the *color* skill and Lambda function example provided by ASK as a template to create the Lambda function for *panel*. Since I my Lambda function needed to talk to a remote Raspberry Pi server, I added that functionality as well as modifying the logic and speech responses to suit my application. One of the biggest challenges I had was where to place to callbacks that returned responses back to Alexa due to the async nature of Node.js. Once I figured that out, the rest was pretty easy. Here's a few key parts of the code which is listed in its entirety [elsewhere](https://github.com/goruck/all/blob/master/lambda/all.js).

The snippet below sets up the ability to use the tls method to open, read, and write a TCP socket that connects to the remote Pi server. the tls method provides both authentication and encryption between the Lambda client and the Pi server. 

```javascript
var tls = require('tls');
var PORT = XXXX;
var HOST = 'XXX.XXX.XXX.XXX'; // todo: use FQDN instead of IP
    
var options = {
   host: HOST,
   port: PORT,
   rejectUnauthorized: false, // danger - MITM attack possible - todo: fix
   key:"", // add for client-side auth - todo: add
   cert:"",
   ca:""
};
```

I found a few challenges here due to using an IP address for the remote client instead of a FQDN and using self-signed TLS certificates. I'm currently working around this by setting *rejectUnauthorized* to false which allows an unauthenticated connection between Lambda client and the remote Pi server. This creates a potential man-in-the-middle vulnerability where the Lambda function could be tricked into connecting to a server other than the remote Pi. A fix for that will be in place soon. I'm also not currently using client side authentication which needs to be added for better end-to-end security, but in order to enable that I need to figure out a way to safely store the client credentials in Lambda which is a challenge since Lambda itself does not offer persistent storage and I don't want to hard code certificate data (in .PEM) in the Node.js code. I think there is a way to do this by storing the cert data in S3 and linking that to Lambda, this is something that I'll try soon. For now, I'm using other methods on the server side to make sure that only the clients I want connect to the Pi.

The snippet below writes a value to the remote Pi server that gets translated into an alarm keypad command. 

```javascript
var socket = tls.connect(options, function() {
   if(socket.authorized){
     console.log('authorized');
   }
   else{
      console.log('cert auth error: ', socket.authorizationError);
   }
   socket.write(num +'\n');
   console.log('wrote ' +num);
   socket.end;
   console.log('disconnected from host ' +HOST);
   callback(sessionAttributes,
      buildSpeechletResponse(intent.name, speechOutput, repromptText, shouldEndSession));
});
```
The snippets below read alarm status coming back from the Pi server and sends it back to Alexa. 

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
The Linux disto on the Pi is based on Debian 7.0 "Wheezy". I used a fork of the [Raspberry Pi Linux kernel](https://github.com/raspberrypi/linux) patched with rt-patch and configured as a fully preemptible kernel. The patched and configured source code for the Raspberry Pi real-time kernel is available on [GitHub](https://github.com/emlid/linux-rt-rpi). I found the wiki [real-time Linux](https://rt.wiki.kernel.org/index.php/Main_Page) to be an invaluable source of information on the rt-patch and how to write real-time code.

The patched version of the kernel includes the following changes.
* Replaced default kernel with PREEMPT_RT kernel 3.18.9-rt5-v7+
* Enabled non-FIQ USB driver (currently FIQ driver is not compatible with RT-patch on RPi2)
* Enabled camera, SPI, I2C and set its speed to 1MHz (if you’d like to connect a sensor with lower clock speed, edit the baudrate option in /etc/modprobe.d/i2c.conf).
* Disabled serial console.
* Changed default WiFi network parameters.

I tested latency of the patched kernel using [Cyclictest](https://rt.wiki.kernel.org/index.php/Cyclictest) and measured a worse case of less than 60 us. So, that seemed to indicate that my applications could respond to events at up to about 15 kHz. I also measured the latency of a vanillia 3.18 kernel on the Pi and got about 20 ms typical. The patched kernel is roughly three orders of magnitude faster!

The distribution of the latency on the patched kernel as measure by Cyclictest is shown below.

![pi latency](https://cloud.githubusercontent.com/assets/12125472/11770022/4d3df3fe-a1a9-11e5-91b7-281b4b064da6.gif)

### Controller / Server Application
The application code on the Pi emulates a DSC keypad controller running in the Linux system's userspace. The application code in its entirety can be found [here](https://github.com/goruck/all/blob/master/rpi/kprw-server.c) and in this section I've tried to capture important design considerations that are not obvious from the code itself or the comments. 

The DSC system is closed and proprietary and besides the installation and programming information in the DSC manuals and what hackers have managed to figured out and posted on the Internet and GitHub, there isn't much information available about the protocol it uses to communicate with its keypads. Also, much of what I found on the Internet relies on using an IP interface bridge like this [one](http://www.eyezon.com/?page_id=176) from Eyezon. I could have used a bridge like this instead of my code on the Raspberry Pi but that would have made my reference design much less generic (and much less fun to develop). I did find a lot of useful information on how to hack the DSC system from the [Arduino-Keybus](https://github.com/emcniece/Arduino-Keybus) GitHub repo, but it used an Arduino which I ruled out (see above) and it was read-only. Still, it was very educational. Although these sources helped, I ended up doing a lot of reverse engineering to figure out the protocol used on the DSC Keybus. This mainly consisted of using an early version of my application code to examine the bits sent from the panel to the keypads and the keypads to the panel in response to keypad button presses and other events, like motion and door / window openings and closings. I found that the first two bytes of each message indicates its type, followed by a variable number of data bits including error protection. For example a message with its first two bytes equal to 0x05 is telling the keypad to illuminate specific status LEDs, such as BYPASS, READY, etc and a message type equal to 0xFF indicates keypad to panel data is being sent. See the *decode()* function in the application code for details regarding the various message types and their contents.

The application code consists of three main parts:

* A thread called *panel_io()* that handles the bit-level processing needed to assemble messages from the keybus serial data. 
* A thread called *msg_io()* that handles the message-level output processing.
* A server called *panserv()* running in the main thread that communicates with an external client for commands and status.

The application code directly accesses the GPIO's registers for the fastest possible reads and writes. I based my code that implements this direct access on [this](http://elinux.org/RPi_GPIO_Code_Samples#Direct_register_access) information from the Embedded Linux Wiki at elinux.org. The function *setup_io()* in the application code sets up a memory regions to access the GPIOs. I found the information in the Broadcom BCM2835 ARM Peripherals document very useful to understand how to safely access the Pi's processor peripherals. You can find it [here](http://elinux.org/RPi_Documentation) on the Embedded Linux Wiki site.

Inter-thread communication is handled safely via read and write FIFOs without any synchronization (e.g., using a mutext). I didn't want to use conventional thread synchronization methods since they would block the *panel_io()* thread. I don't have any way to tell the panel not to send data on the keybus, so if *panel_io()* was blocked by another thread accessing a shared FIFO, the application code would drop messages. I found a good solution to this problem in the article [Creating a Thread Safe Producer Consumer Queue in C++ Without Using Locks](http://blogs.msmvps.com/vandooren) by Vandooren, which is what I implemented with a few modifications.

Running code under real-time Linux requires a few special considerations. Again, I used the [real-time Linux](https://rt.wiki.kernel.org/index.php/Main_Page) wiki extensively to guide my efforts in this regard. I also used the excellent book [The Linux Programming Interface](http://man7.org/tlpi/) by Kerrisk to help me understand more deeply how the Linux kernel schedules tasks. I learned that there are three things needed to be set by a task in order to provide deterministic real time behavior:

1. Setting a real time scheduling policy and priority by use of the *sched_setscheduler()* system call and its pthread cousins. Note that *panel_io()*, *message_io()*, and *main()* were all given the same scheduling policy (SCHED_FIFO) but I gave *panel_io()* a higher priority than *message_io()* and *main()*. I did that to make sure the bit-level processing had enough time to complete, but I did run into a complication described below that I solved by attaching a CPU affinity to the threads.
2. Locking memory so that page faults caused by virtual memory will not undermine deterministic behavior. This is done with the *mlockall()* system call. 
3. Pre-faulting the stack, so that a future stack fault will not undermine deterministic behavior. This is done by simply accessing each element of the program's stack by use of the *memset()* system call. 

I made extensive use of the *clock_nanosleep()* function from librt. For example, its use is shown in the *panel_io()* thread where it awakens the thread every INTERVAL (10 us) to read and write to the GPIOs driving the keybus clock and data lines (via the interface unit). The thread contains logic to detect the start of word marker, to detect high and low clock periods, and to control precisely when the GPIOs are read and written. The snippet below taken from *panel_io()* causes the thread to wait for valid data before reading the GPIO.

```c
t.tv_nsec += KSAMPLE_OFFSET;
tnorm(&t);
clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL); // wait KSAMPLE_OFFSET for valid data
wordkr_temp = (GET_GPIO(PI_DATA_IN) == PI_DATA_HI) ? '0' : '1'; // invert
```

Even though the *panel_io()* is given higher priority than the other two threads, other critical threads in the Linux kernel must have higher still priority for the overall system to function. So I was concerned that a higher priority kernel task could cause *panel_io()* to drop bits. I consulted the real-time Linux wiki for a solution and found this:

*"In general, I/O is dangerous to keep in an RT code path. This is due to the nature of most filesystems and the fact that I/O devices will have to abide to the laws of physics (mechanical movement, voltage adjustments, <whatever an I/O device does to retrieve the magic bits from cold storage>). For this reason, if you have to access I/O in an RT-application, make sure to wrap it securely in a dedicated thread running on a disjoint CPU from the RT-application."* 

So I decided to lock the *panel_io()* thread to its own CPU and let the other threads have free reign on the other three CPUs (its really great to have four CPUs at one's disposal). The application code sets the thread CPU affinity as shown below. 

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

The server uses openSSL for authentication and encryption. For development and test purposes, I used self-signed certificates and the IP address of the server instead of a Fully Qualified Domain Name. I generated the cert and private key with the following commands.

```bash
$ openssl genrsa -out privateKey.key 2048
$ openssl req -new -key privateKey.key -out privateKey.csr
$ openssl x509 -req -in privateKey.csr -signkey privateKey.key -out certificate.crt -days 500 -extfile key.ext
```
Where key.ext contains "subjectAltName = IP:xxx.xxx.xxx.xxx" (replace with the IP address of your server). 

Production code should use certificates signed by a real CA and a FQDN for the server, registered with a DNS. The server also uses TCP Wrapper daemon for secure access. TCP Wrapper uses the *hosts_ctl()* system call from libwrap library to limit client access via the rules defined in /etc/host.deny and /etc/host.allow files. I set the rules so that only clients with local IP addresses and AWS IP addresses are allowed access to the server.

The application code needs to be compiled with the relevant libraries and executed with su privileges, per the following.

```bash
$ gcc -Wall -o kprw-server kprw-server.c -lrt -lpthread -lwrap -lssl -lcrypto
$ sudo ./kprw-server
```

### Startup
The Raspberry Pi is used here as an embedded system so it needs to come up automatically after power on, including after a possible loss of power. The application code defines the GPIOs as follows:

```c
#define PI_CLOCK_IN	(13)
#define PI_DATA_IN	(5)
#define PI_DATA_OUT	(6)
```

So I had to make sure these GPIOs would be in a safe state after power on and boot up. Per the Broadcom BCM2835 ARM Peripherals document, these GPIOs are configured as inputs at reset and the kernel doesn't change that during boot, so they won't cause any the keybus serial data line to be pulled down before the application code initializes them.

I added the code below to /etc/rc.local so that the application code would automatically run after powering on the Pi.

```bash
/home/pi/all/rpi/kprw-server > /dev/null &
```

At some point I'll add some provisions to automatically restart the application code in the event of a crash.

I've seen cases where the Pi's wifi would not automatically reconnect after it loses the connection. To address this, I created the script /usr/local/bin/wifi_rebooter.sh which is periodically by a cron job. The script and cron entry is shown below.

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

```bash
*/5 * * * *   root    /usr/local/bin/wifi_rebooter.sh
```

## Keybus to GPIO Interface Unit

### Keybus Electrical and Timing Charateristics
The keybus is a DSC proprietary serial bus that runs from the panel to the sensors and controller keypads in the house. I first had to understand this bus from an electrical, timing, and protocol point of view (see above) before I could interface the Pi to the panel. There isn't a lot of information on the keybus other than what's in the DSC installation manual and miscellaneous info on the Internet posted by fellow hackers. This is a two wire bus with clock and data, plus ground and the supply from the panel. The supply is 13.8 V (nominal) and the clock and data transition between 0 and 13.8 V. The large voltage swings make sense given that it provides good noise immunity against interference picked up from long runs of the bus through the house. The DSC manual states the bus supply can source a max of 550 mA. I added up the current from all the devices presently on the bus which came out to about 400 mA. This meant that my interface unit could draw no more than 150 mA. I kept that in mind as I designed the circuitry. 

I used an oscilloscope to reverse engineer the protocol. Some screen-shots and my analysis from them are below.

![whole-word-ann](https://cloud.githubusercontent.com/assets/12125472/11801916/17eb6cf8-a29f-11e5-8d3c-5cd4d39ac7ed.gif)
This shows an entire word with the start of new word marker, which is the clock being held high for a relatively long time. The clock rate when active is 1 KHz. The messages between the panel and keypads are of variable length. The typical message from the panel to the keypads is 43 bits (43 ms in duration) and the start of the new word clock marker is about 15 ms. Thus, a typical message time including start of new marker is about 58 ms. I've seen messages up to 62 bits long but they are not frequent. 

![data-clock-close](https://cloud.githubusercontent.com/assets/12125472/11801938/55c9817c-a29f-11e5-82a7-1eac18a5abc4.gif)
This shows a closer view of the clock and data, with data being sent between the panel and keypad and vise-versa. Data from the panel to the keypads transitions on the rising edge of the clock and becomes valid about 120 us later. Data from the keypads to the panel transitions at falling edge of the clock is likely registered in the panel on the next rising edge of the clock. The keypad data needs to be held for at least 25 us after the rising edge of the clock for it to be properly registered. I had to carefully set timers in the Pi's application code to adhere to these values for reliable data transfers to happen. The spikes at 2.5 and 4.5 ms are most likely due to the pull-up resistor on the data line in the panel causing it to be pulled high before the driver on the keypad has a chance to pull it low, which happens when the keypad sends a 0 bit to the panel.

From this data, I was reasonably able to conclude that the panel's data line is bidirectional (with direction switched by clock level) and pulled up internally when configured as an input. I needed to find out the strength of the pull-up in order to design the interface circuit correctly, so I did some more experiments and determined that it was approximately 5 KΩ.

### Raspberry Pi GPIO Electrical Characteristics
Now that I understood the panel side a bit better, I needed to get some more data on the electrical characteristics of the Pi's GPIOs. I could not really find the level of detail that I wanted from the Raspberry Pi standard information. But I did find some excellent information on the Mosaic Industries website [here](http://www.mosaic-industries.com/embedded-systems/microcontroller-projects/raspberry-pi/gpio-pin-electrical-specifications). I found this information to be super helpful in designing an interface that I was confident in. I've pasted some of the key points from the article below. 

* These are 3.3 volt logic pins. A voltage near 3.3 V is interpreted as a logic one while a voltage near zero volts is a logic zero. A GPIO pin should never be connected to a voltage source greater than 3.3V or less than 0V, as prompt damage to the chip may occur as the input pin substrate diodes  conduct. There may be times when you may need to connect them to out ­of ­range voltages – in those cases the input pin current must be limited by an external resistor to a value that prevents harm to the chip. I recommend that you never source or sink more than 0.5 mA into an input pin.
* To prevent excessive power dissipation in the chip, you should not source/sink more current from the pin than its programmed limit. So, if you have set the current capability to 2 mA, do not draw more than 2 mA from the pin.
* Never demand that any output pin source or sink more than 16 mA.
* Current sourced by the outputs is drawn from the 3.3 V supply, which can supply only 50 mA maximum. Consequently, the maximum you can source from all the GPIO outputs simultaneously is less than 50 mA. You may be able to draw transient currents beyond that limit as they are drawn from the bypass capacitors on the 3.3 V rail, but don't push the envelope! There isn't a similar limitation on sink current. You can sink up to 16 mA each into any number of GPIO pins simultaneously. However, transient sink currents do make demands on the board's bypass capacitors, so you may get into trouble if all outputs switch their maximum current synchronously.
* Do not drive capacitive loads. Do not place a capacitive load directly across the pin. Limit current into any capacitive load to a maximum transient current of 16 mA. For example, if you use a low pass filter on an output pin, you must provide a series resistance of at least 3.3V/16mA = 200 Ω.

### Interface Circuit Design
Now that I had a good understanding of the panel keybus and Raspberry PI GPIO electrical characteristics, I could finally design the interface circuity. Since I wanted to electrically isolate the panel and Pi to prevent ground loops, optoisolators were an obvious choice. But they do consume a fair amount of current so I had to buffer them instead of directly connecting them between the keybus clock and data and PI GPIOs. This added a bit more complexity but it was the only way to keep the current consumption within range of both the panel and PI's capabilities. I had to use a high Current Transfer Ratio (CTR) optoisolator configured actively to drive a logic-level FET to pull down the data line with sufficient strength, the other optoisolaors are not high CTR (high CTR = $$). The interface schematic is shown below (at some point I will convert my hand drawing to a real CAD schematic). 

![netbus-gpio-if2](https://cloud.githubusercontent.com/assets/12125472/11706723/416ad890-9eb0-11e5-976f-e48f492587b6.png)
![netbus-gpio-if1](https://cloud.githubusercontent.com/assets/12125472/11706728/47b519f4-9eb0-11e5-8f12-56c11d6d14d0.png)
Note: bypass capacitors not shown. 

# Development and Test environment
TBA

# Appendix

## DSC Power832 Overview

## Bill of materials and service cost considerations
TBA

## Proof of Concept Output to Terminal
![screenshot from 2015-12-09 19 46 40](https://cloud.githubusercontent.com/assets/12125472/11706514/3819320c-9eae-11e5-95d0-af8bdee6ed24.png)
Note: connections to AWS Lambda triggered by Alexa

## Proof of Concept Current State
![proto pic1](https://cloud.githubusercontent.com/assets/12125472/11706674/a8721fcc-9eaf-11e5-8707-f780ae4ef86a.png)
![proto pic2](https://cloud.githubusercontent.com/assets/12125472/11706679/b0fb6950-9eaf-11e5-95be-3d668412c5e2.png)
Note: planning to move interface circuits from breadboard to board that fits in Raspberry PI housing 

## PDFs of Block Diagram and System Overview
[all blockdia.pdf](https://github.com/goruck/all/files/57052/all.blockdia.pdf)
[all overview.pdf](https://github.com/goruck/all/files/57059/all.overview.pdf)
 
<a name="myfootnote1">1</a>: Footnote content about measuring GPIO jitter under Linux goes here
<a name="myfootnote2">2</a>: Although this looks very waterfall-ish, in reality I iterated between architecture / design / test to arrive at the final systems architecture.
<a name="myfootnote3">3</a>: Footnote content about Alexa bug responding to other utterances goes here
