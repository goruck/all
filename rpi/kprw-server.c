/*
 *
 * kprw-server.c, V2.0
 *
 * Emulates a DSC Power832 keypad controller. Reads and writes over the keybus are supported.
 *
 * This version supports machine learning via R. 
 *
 * Compile with "gcc -Wall -o kprw-server kprw-server.c -lrt -lpthread -lwrap -lssl -lcrypto"
 *
 * Tested with Raspberry Pi 2.
 *
 * Must run under linux PREEMPT_RT kernel 3.18.9-rt5-v7 and as su.
 *
 * See https://github.com/goruck/mall for details. 
 *
 * Lindo St. Angel 2015/16. 
 *
 */

#define _GNU_SOURCE // Need to use some non-portable pthread functions

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>		// Needed for file I/O
#include <sys/mman.h>		// Needed for mlockall()
#include <unistd.h>
#include <stdint.h>
#include <sched.h>
#include <string.h>
#include <time.h> 		// Needed for getrusage()
#include <sys/resource.h>	// Needed for getrusage()
#include <pthread.h>
#include <sys/utsname.h>
#include <ctype.h>		// Needed for isdigit()
#include <malloc.h>		// Needed for mallopt()

// socket
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <tcpd.h> //for hosts_ctl()
#include <netdb.h>
#define	BUF_LEN		16  // size of string to hold longest message incl '\n'
#define BACKLOG		1   // only allow one client to connect
//#define	_BSD_SOURCE // to get definitions of NI_MAXHOST and NI_MAXSERV from <netdb.h>
#define ADDRSTRLEN	(NI_MAXHOST + NI_MAXSERV + 10)

// openssl
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>

// GPIO Access from ARM Running Linux. Based on Dom and Gert rev 15-feb-13
#define BCM2708_PERI_BASE 0x3F000000 // modified for Pi 2
#define GPIO_BASE	  (BCM2708_PERI_BASE + 0x200000) // GPIO controller
#define BLOCK_SIZE	  (4*1024) // size of memory for direct gpio access

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y).
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))
#define GPIO_SET *(gpio+7)  // sets bit by writing a 1, writing a 0 has no effect (BCM Set 0)
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0 (BCM Clear 0)
#define GET_GPIO(g) (*(gpio+13)&(1<<g)) // 0 if LOW, (1<<g) if HIGH (BCM Level 0)
#define GPIO_EVENT *(gpio+16) // 0 if no event, (1<<g) if event (BCM Event Detect Status 0)
#define ENB_GPIO_REDGE *(gpio+19) //Rising Edge Detect Enable 0
#define ENB_GPIO_FEDGE *(gpio+22) //Falling Edge Detect Enable
#define ENB_GPIO_HIDET *(gpio+25) //High Detect Enable 0
#define ENB_GPIO_LODET *(gpio+28) //Low Detect Enable 0
#define GPIO_PULL *(gpio+37) // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio+38) // Pull up/pull down clock (BCM Clock 0)

// GPIO pin mapping.
#define PI_CLOCK_IN	(13) // BRCM GPIO13 / PI J8 Pin 33
#define PI_DATA_IN	(5)  // BRCM GPIO05 / PI J8 Pin 29
#define PI_DATA_OUT	(16) // BRCM GPIO16 / PI J8 Pin 36

// GPIO high and low level mapping macros.
#define PI_CLOCK_HI (1<<PI_CLOCK_IN)
#define PI_CLOCK_LO (0)
#define PI_DATA_HI  (1<<PI_DATA_IN)
#define PI_DATA_LO  (0)

// GPIO invert macro
#define INV(g,s)	((1<<g) - s)

// real-time
#define MAIN_PRI	(70) // main thread priority
#define MSG_IO_PRI	(70) // message io thread priority
#define PREDICT_PRI	(50) // predict thread priority
#define PANEL_IO_PRI	(90) // panel io thread priority - panel io pri must be highest
#define MAX_SAFE_STACK	(32*1024*1024) // 32MB pagefault free buffer
#define MY_STACK_SIZE   (100*1024) // 100KB thread stack size
#define NSEC_PER_SEC    (1000000000LU) // 1 second.
#define INTERVAL        (10*1000) // 10 us timeslice.
#define CLK_PER		(1000000L) // 1 ms clock period.
#define HALF_CLK_PER	(500000L) // 0.5 ms half clock period.
#define SAMPLE_OFFSET   (120000L) // 0.12 ms sample offset from clk edge for panel read
#define KSAMPLE_OFFSET	(300000L) // 0.30 ms sample offset from clk edge for keypad read
#define HOLD_DATA	(220000L) // 0.22 ms data hold time from clk edge for keypad write
#define CLK_BLANK	(5000000L) // 5 ms min clock blank.
#define NEW_WORD_VALID	(2500000L) // if a bit arrives > 2.5 ms after last one, declare start of new word.
#define MAX_BITS	(64) // max 64-bit word read from panel
#define MAX_DATA 	(1*1024) // 1 KB data buffer of 64-bit data words - ~66 seconds @ 1 kHz.
#define FIFO_SIZE	(MAX_BITS*MAX_DATA) // FIFO depth
#define MSG_IO_UPDATE   (5000000) // 5 ms message io thread update period
#define PREDICT_UPDATE  (NSEC_PER_SEC) // 1 sec predict thread update period

// keypad button bit mappings
// no button:	0xff 0xff 0xff 0xff 0xff 0xff 0xff 0xff
#define IDLE	"1111111111111111111111111111111111111111111111111111111111111111"
// *:		0xff 0x94 0x7f 0xff 0xff 0xff 0xff 0xff        
#define STAR	"1111111110010100011111111111111111111111111111111111111111111111"
// #:		0xff 0x96 0xff 0xff 0xff 0xff 0xff 0xff
#define POUND	"1111111110010110111111111111111111111111111111111111111111111111"
// 0:		0xff 0x80 0x7f 0xff 0xff 0xff 0xff 0xff
#define ZERO	"1111111110000000011111111111111111111111111111111111111111111111"
// 1:		0xff 0x82 0xff 0xff 0xff 0xff 0xff 0xff
#define ONE	"1111111110000010111111111111111111111111111111111111111111111111"
// 2:		0xff 0x85 0x7f 0xff 0xff 0xff 0xff 0xff
#define TWO	"1111111110000101011111111111111111111111111111111111111111111111"
// 3:		0xff 0x87 0xff 0xff 0xff 0xff 0xff 0xff
#define THREE	"1111111110000111011111111111111111111111111111111111111111111111"
// 4: 		0xff 0x88 0xff 0xff 0xff 0xff 0xff 0xff
#define FOUR	"1111111110001000111111111111111111111111111111111111111111111111"
// 5: 		0xff 0x8b 0x7f 0xff 0xff 0xff 0xff 0xff
#define FIVE	"1111111110001011100011111111111111111111111111111111111111111111"
// 6:		0xff 0x8d 0xff 0xff 0xff 0xff 0xff 0xff
#define SIX	"1111111110001101111111111111111111111111111111111111111111111111"
// 7: 		0xff 0x8e 0x7f 0xff 0xff 0xff 0xff 0xff
#define SEVEN	"1111111110001110111111111111111111111111111111111111111111111111"
// 8: 		0xff 0x91 0x7f 0xff 0xff 0xff 0xff 0xff
#define EIGHT	"1111111110010001011111111111111111111111111111111111111111111111"
// 9: 		0xff 0x93 0xff 0xff 0xff 0xff 0xff 0xff
#define NINE	"1111111110010011111111111111111111111111111111111111111111111111"
// stay:        0xff 0xd7 0xff 0xff 0xff 0xff 0xff 0xff
#define STAY	"1111111111010111111111111111111111111111111111111111111111111111"
// away:	0xff 0xd8 0xff 0xff 0xff 0xff 0xff 0xff
#define AWAY	"1111111111011000111111111111111111111111111111111111111111111111"

// Rscript
#define POPEN_FMT     "/home/pi/R_HOME/R-3.1.2/bin/Rscript --vanilla /home/pi/all/R/predknn.R %s %s %s 2> /dev/null"
#define RARG_SIZE     128
#define ROUT_MAX      128
#define PCMD_BUF_SIZE (sizeof(POPEN_FMT) + RARG_SIZE)
#define RLOGPATH      "/home/pi/all/R/rlog.txt"

// structure to hold a snapshot of the panel status and sensor observations
struct status {
  char ledStatus[50];          // panel main led status lights
  char zone1Status[50];        // panel zone 1 status lights
  char zone2Status[50];        // panel zone 2 status lights
  char zone3Status[50];        // panel zone 3 status lights
  char zone4Status[50];        // panel zone 4 status lights
  long unsigned obsTime;       // zone sensor absolute observation time
  long unsigned zoneAct[32];   // zone sensor absolute activation times
  long unsigned zoneDeAct[32]; // zone sensor absolute deactivation times
};

// global for direct gpio access
volatile unsigned *gpio;

// fifo globals
volatile int m_Read1, m_Write1, m_Read2, m_Write2;
volatile char m_Data1[FIFO_SIZE], m_Data2[FIFO_SIZE];

// show_new_pagefault_count
static void show_new_pagefault_count(const char* logtext, 
   			      const char* allowed_maj,
   			      const char* allowed_min) {
  static int last_majflt = 0, last_minflt = 0;
  struct rusage usage;
   
  getrusage(RUSAGE_SELF, &usage);
   
  fprintf(stdout, "%-30.30s: Pagefaults, Major:%ld (Allowed %s), " \
   	 "Minor:%ld (Allowed %s)\n", logtext,
   	 usage.ru_majflt - last_majflt, allowed_maj,
   	 usage.ru_minflt - last_minflt, allowed_min);
   	
  last_majflt = usage.ru_majflt; 
  last_minflt = usage.ru_minflt;
} // show_new_pagefault_count

/* prove_thread_stack_use_is_safe
 *
 * Note: gcc -Wall will complain here that buffer is set but not used. 
 * This is because buffer is a local variable on the stack
 *   but not used outside the scope of this function.
 * This is why buffer is a volatile because otherwise complier would optimize it away. 
 */
static void prove_thread_stack_use_is_safe(int stacksize) {
  volatile char buffer[stacksize];
  int i;
   
  // Prove that this thread is behaving well
  for (i = 0; i < stacksize; i += sysconf(_SC_PAGESIZE)) {
    // Each write to this buffer shall NOT generate a pagefault.
    buffer[i] = i;
  }
   
  show_new_pagefault_count("Caused by using thread stack", "0", "0");
} // prove_thread_stack_use_is_safe

// reserve_process_memory
static void reserve_process_memory(int size) {
  int i;
  char *buffer;
   
  buffer = malloc(size);
   
  // Touch each page in this piece of memory to get it mapped into RAM
  for (i = 0; i < size; i += sysconf(_SC_PAGESIZE)) {
    /* Each write to this buffer will generate a pagefault.
       Once the pagefault is handled a page will be locked in
       memory and never given back to the system. */
    buffer[i] = 0;
  }
   
  /* buffer will now be released. As Glibc is configured such that it 
  never gives back memory to the kernel, the memory allocated above is
  locked for this process. All malloc() and new() calls come from
  the memory pool reserved and locked above. Issuing free() and
  delete() does NOT make this locking undone. So, with this locking
  mechanism we can build C/C++ applications that will never run into
  a major/minor pagefault, even with swapping enabled. */
  free(buffer);

  return;
} // reserve_process_memory

// Set up a memory regions to access GPIO
static void setup_io(void) {
  int  mem_fd;
  void *gpio_map;

  // open /dev/mem
  if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
    perror("can't open /dev/mem \n");
    exit(EXIT_FAILURE);
  }

  // mmap GPIO
  gpio_map = mmap(
    NULL,             		//Any adddress in our space will do
    BLOCK_SIZE,			//Map length
    PROT_READ|PROT_WRITE,	//Enable reading & writting to mapped memory
    MAP_SHARED,			//Shared with other processes
    mem_fd,           		//File to map
    GPIO_BASE         		//Offset to GPIO peripheral
  );

  close(mem_fd); // No need to keep mem_fd open after mmap

  if (gpio_map == MAP_FAILED) {
    fprintf(stderr, "mmap error %d\n", (int) gpio_map); //errno also set!
    exit(EXIT_FAILURE);
  }

  // Always use volatile pointer!
  gpio = (volatile unsigned *)gpio_map;

  return;

} // setup_io

/* using clock_nanosleep of librt */
extern int clock_nanosleep(clockid_t __clock_id, int __flags,
                           __const struct timespec *__req,
                           struct timespec *__rem);

/* 
 * the struct timespec consists of nanoseconds
 * and seconds. if the nanoseconds are getting
 * bigger than 1000000000 (= 1 second) the
 * variable containing seconds has to be
 * incremented and the nanoseconds decremented
 * by 1000000000.
 */
static inline void tnorm(struct timespec *tp)
{
   while (tp->tv_nsec >= NSEC_PER_SEC) {
      tp->tv_nsec -= NSEC_PER_SEC;
      tp->tv_sec++;
   }
}

// Wait for a change in clock level and measure the time it took.
static inline unsigned long waitCLKchange(struct timespec *tp, int currentState)
{
  unsigned long c = 0;

  while (GET_GPIO(PI_CLOCK_IN) == currentState) {
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, tp, NULL);
    tp->tv_nsec += INTERVAL;
    tnorm(tp);
    c += INTERVAL;
  }

  return c; // time between change in nanoseconds
} // waitCLKchange

static inline long ts_diff(struct timespec *a, struct timespec *b)
{
  long x, y;

  x = (a->tv_sec)*NSEC_PER_SEC + a->tv_nsec;
  y = (b->tv_sec)*NSEC_PER_SEC + b->tv_nsec;

  return (x - y);
}

static inline unsigned int getBinaryData(char *st, int offset, int length)
{
  unsigned int buf = 0, j;

  for (j = 0; j< length; j++) {
    buf <<=1;
    if ( *(st + offset + j) == '1' ) buf |= 1;
  }

  return buf;
}

/* 
 * Fifos are thread safe without using any sychronization (e.g., mutext).
 * But each fifo must have exactly one producer and one consumer to be used safely.
 * See "Creating a Thread Safe Producer Consumer Queue in C++ Without Using Locks",
 * http://blogs.msmvps.com/vandooren
 */

// fifo1 - stores panel to keypad and keypad to panel data
static inline int pushElement1(char *element, int num) {
  int nextElement, i;

  // increment or reset pointer
  nextElement = ((m_Write1 + num) >= FIFO_SIZE) ? 0 : (m_Write1 + num);

  if (nextElement != m_Read1) { // fifo not full
    for (i = 0; i < num; i++) {
      m_Data1[m_Write1 + i] = element[i];
    }
  }
  
  // if fifo was full, data will be overwritten
  m_Write1 = nextElement;

  return i; // return number of elements pushed
}

static inline int popElement1(char *element, int num) {
  int nextElement, i;

  if (m_Read1 == m_Write1) {
    return 0; // fifo is empty
  }

  // increment or reset pointer
  nextElement = ((m_Read1 + num) >= FIFO_SIZE) ? 0 : (m_Read1 + num);

  for (i = 0; i < num; i++) {
    element[i] = m_Data1[m_Read1 + i];
  }

  m_Read1 = nextElement;

  return i;
}

// fifo2 - stores keypad data to be sent to panel
static inline int pushElement2(char *element, int num) {
  int nextElement, i;

  // increment or reset pointer
  nextElement = ((m_Write2 + num) >= FIFO_SIZE) ? 0 : (m_Write2 + num);

  if (nextElement != m_Read2) {
    for (i = 0; i < num; i++) {
      m_Data2[m_Write2 + i] = element[i];
    }
  } else {
    return 0; // fifo is full, data not overwritten
  }

  m_Write2 = nextElement;

  return i;
}

static inline int popElement2(char *element, int num) {
  int nextElement, i;

  if (m_Read2 == m_Write2) {
    return 0; // fifo is empty
  }

  // increment or reset pointer
  nextElement = ((m_Read2 + num) >= FIFO_SIZE) ? 0 : (m_Read2 + num);

  for (i = 0; i < num; i++) {
    element[i] = m_Data2[m_Read2 + i];
  }

  m_Read2 = nextElement;

  return i;
}

// Decode bits from panel into commands and messages.
static int decode(char * word, char * msg, int * allZones) {
  int cmd = 0, zones = 0, button = 0, i = 0;
  char year3[2],year4[2],month[2],day[2],hour[2],minute[2],str[2];

  cmd = getBinaryData(word,0,8);
  strcpy(msg, "");

  if (cmd == 0x05) { 
    strcpy(msg, "LED Status ");
    if (getBinaryData(word,16,1))
      strcat(msg, "Ready, ");
    else
      strcat(msg, "Not Ready, ");
    if (getBinaryData(word,12,1)) strcat(msg, "Error, ");
    if (getBinaryData(word,13,1)) strcat(msg, "Bypass, ");
    if (getBinaryData(word,14,1)) strcat(msg, "Memory, ");
    if (getBinaryData(word,15,1)) strcat(msg, "Armed, ");
    if (getBinaryData(word,17,1)) strcat(msg, "Program, ");
  }
  else if (cmd == 0xa5) {
    sprintf(year3, "%d", getBinaryData(word,9,4));
    sprintf(year4, "%d", getBinaryData(word,13,4));
    sprintf(month, "%d", getBinaryData(word,19,4));
    sprintf(day, "%d", getBinaryData(word,23,5));
    sprintf(hour, "%d", getBinaryData(word,28,5));
    sprintf(minute, "%d", getBinaryData(word,33,6));
    strcpy(msg, "Date: 20");
    strcat(msg, year3);
    strcat(msg, year4);
    strcat(msg, "-");
    strcat(msg, month);
    strcat(msg, "-");
    strcat(msg, day);
    strcat(msg, " ");
    strcat(msg, hour);
    strcat(msg, ":");
    strcat(msg, minute);
  }
  else if (cmd == 0x27) {
    strcpy(msg, "Zone1 ");
    zones = getBinaryData(word,41,8);
    for (i = 0; i < 8; i++) {
      sprintf(str, "%d, ", i + 1);
      if (zones & (1 << i)) {
        *(allZones + i) = 1;
        strcat(msg, str);
      } else {
        *(allZones + i) = 0;
      }
    }
    if (!zones) strcat(msg, "Ready ");
  }
  else if (cmd == 0x2d) {
    strcpy(msg, "Zone2 ");
    zones = getBinaryData(word,41,8);
    for (i = 0; i < 8; i++) {
      sprintf(str, "%d, ", i + 1);
      if (zones & (1 << i)) {
        *(allZones + i + 8) = 1;
        strcat(msg, str);
      } else {
        *(allZones + i + 8) = 0;
      }
    }
    if (!zones) strcat(msg, "Ready ");
  }
  else if (cmd == 0x34) {
    strcpy(msg, "Zone3 ");
    zones = getBinaryData(word,41,8);
    for (i = 0; i < 8; i++) {
      sprintf(str, "%d, ", i + 1);
      if (zones & (1 << i)) {
        *(allZones + i + 16) = 1;
        strcat(msg, str);
      } else {
        *(allZones + i + 16) = 0;
      }
    }
    if (!zones) strcat(msg, "Ready ");
  }
  else if (cmd == 0x3e) {
    strcpy(msg, "Zone4 ");
    zones = getBinaryData(word,41,8);
    for (i = 0; i < 8; i++) {
      sprintf(str, "%d, ", i + 1);
      if (zones & (1 << i)) {
        *(allZones + i + 24) = 1;
        strcat(msg, str);
      } else {
        *(allZones + i + 24) = 0;
      }
    }
    if (!zones) strcat(msg, "Ready ");
  }
  else if (cmd == 0x0a)
    strcpy(msg, "Panel Program Mode");
  else if (cmd == 0x63)
    strcpy(msg, "Undefined command from panel");
  else if (cmd == 0x64)
    strcpy(msg, "Undefined command from panel");
  else if (cmd == 0x69)
    strcpy(msg, "Undefined command from panel");
  else if (cmd == 0x5d)
    strcpy(msg, "Undefined command from panel");
  else if (cmd == 0x39)
    strcpy(msg, "Undefined command from panel");
  else if (cmd == 0xb1)
    strcpy(msg, "Undefined command from panel");
  else if (cmd == 0x11)
    strcpy(msg, "Keypad query");
  else if (cmd == 0xff) { // keypad to panel data
    strcpy(msg, "From Keypad ");
    if (getBinaryData(word,8,32) == 0xffffffff)
      strcat(msg, "idle");
    else { //bits 11~14 data; 15~16 CRC (not used)
      button = getBinaryData(word,8,20); 
      if (button == 0x947ff)
        strcat(msg, "button * pressed");
      else if (button == 0x96fff)
        strcat(msg, "button # pressed");
      else if (button == 0x807ff)
        strcat(msg, "button 0 pressed");
      else if (button == 0x82fff)
        strcat(msg, "button 1 pressed");
      else if (button == 0x857ff)
        strcat(msg, "button 2 pressed");
      else if (button == 0x87fff)
        strcat(msg, "button 3 pressed");
      else if (button == 0x88fff)
        strcat(msg, "button 4 pressed");
      else if (button == 0x8b7ff)
        strcat(msg, "button 5 pressed");
      else if (button == 0x8dfff)
        strcat(msg, "button 6 pressed");
      else if (button == 0x8e7ff)
        strcat(msg, "button 7 pressed");
      else if (button == 0x917ff)
        strcat(msg, "button 8 pressed");
      else if (button == 0x93fff)
        strcat(msg, "button 9 pressed");
      else if (button == 0xd7fff)
        strcat(msg, "stay button pressed");
      else if (button == 0xd8fff)
        strcat(msg, "away button pressed");
      else
        strcat(msg, "unknown keypad msg");
    }
  }
  else
    strcpy(msg, "Unknown command from panel");

  return cmd; // return command associated with the message

} // decode

/*
 * panel io thread
 * Every INTERVAL seconds, this thread reads and writes bits to the panel's keybus interface.
 * The read bits are assembled into messages that get decoded by the message i/o thread.
 *
 */
static void * panel_io(void *arg) {
  char word[MAX_BITS] = "", wordkw[MAX_BITS], wordkr[MAX_BITS] ="", wordkr_temp = '0';
  int flag = 0, bit_cnt = 0, res;
  struct timespec t, tmark;

  // detach the thread since we don't care about its return status
  res = pthread_detach(pthread_self());
  if (res) {
    perror("panel i/o thread detach failed\n");
    exit(EXIT_FAILURE);
  }

  strncpy(wordkw, IDLE, MAX_BITS);
  clock_gettime(CLOCK_MONOTONIC, &t);
  tmark = t;
  while (1) {
    t.tv_nsec += INTERVAL;
    tnorm(&t);
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

    if ((GET_GPIO(PI_CLOCK_IN) == PI_CLOCK_HI) && !flag) { // write/read keypad data
      if (ts_diff(&t, &tmark) > NEW_WORD_VALID) { // check for new word
        /* 
         * Check to see if last word was less than 20 bits.
         * Consider words with fewer than 20 bits to be invalid. 
         * If invalid, repeat last keypad write by not fetching new data from fifo.
         * Also, do not store either panel or keypad data.
         *
         * Invalid words may be due to a real-time task with higher prority
         *   than this thread preempting it, or latencies caused by page faults.
         * Despite best efforts to make ensure robust real-time performance
         *   these error checks are still required to be 100% safe. 
         *
         * Panel also outputs a 9-bit word which is ignored as invalid
         *   because thread needs at least 20 bits to send a valid data word to keypad. 
         */
        if (bit_cnt < 20) {
          fprintf(stderr, "panel_io: bit count < 20 (%i)! Repeating panel writes and ignoring reads.\n", bit_cnt);
        } else {
          res = pushElement1(word, MAX_BITS); // store panel-> keypad data
          if (res != MAX_BITS) {
            fprintf(stderr, "panel_io: fifo write error\n"); // record error and continue
          }

          res = pushElement1(wordkr, MAX_BITS); // store keypad-> panel data
          if (res != MAX_BITS) {
            fprintf(stderr, "panel_io: fifo write error\n");
          }

          res = popElement2(wordkw, MAX_BITS); // get a keypad command to send to panel
          if (res != MAX_BITS) { // fifo is empty so output idle instead of repeating previous
            strncpy(wordkw, IDLE, MAX_BITS);
          }
        }

        // reset bit counter and arrays
        bit_cnt = 0; 
        memset(&word, 0, MAX_BITS);
        memset(&wordkr, 0, MAX_BITS);
      }

      tmark = t; // mark new word time
      flag = 1; // set flag to indicate clock was high

      // write keypad data bit to panel once every time clock is high
      if (wordkw[bit_cnt] == '0') // invert
        GPIO_SET = 1<<PI_DATA_OUT; // set GPIO
      else if (wordkw[bit_cnt] == '1') // invert
        GPIO_CLR = 1<<PI_DATA_OUT; // clear GPIO
      else {
        GPIO_CLR = 1<<PI_DATA_OUT; // clear GPIO
        fprintf(stderr, "panel_io: bad element in keypad data array wordk\n");
        //exit(EXIT_FAILURE);
      }

      // read keypad data, including that just written
      t.tv_nsec += KSAMPLE_OFFSET;
      tnorm(&t);
      clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL); // wait KSAMPLE_OFFSET for valid data
      wordkr_temp = (GET_GPIO(PI_DATA_IN) == PI_DATA_HI) ? '0' : '1'; // invert

      t.tv_nsec += HOLD_DATA;
      tnorm(&t);
      clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL); // wait HOLD_DATA time
      GPIO_CLR = 1<<PI_DATA_OUT; // leave with GPIO cleared
    }
    else if ((GET_GPIO(PI_CLOCK_IN) == PI_CLOCK_LO) && flag) { // read panel data
      flag = 0;

      t.tv_nsec += SAMPLE_OFFSET;
      tnorm(&t);
      clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL); // wait SAMPLE_OFFSET for valid data
      wordkr[bit_cnt] = wordkr_temp;
      word[bit_cnt++] = (GET_GPIO(PI_DATA_IN) == PI_DATA_HI) ? '0' : '1'; // invert

      if (bit_cnt >= MAX_BITS) bit_cnt = (MAX_BITS - 1); // never let bit_cnt exceed MAX_BITS
    }
  }
} // panel_io thread

/* 
 * message i/o thread
 * This thread runs every MSG_IO_UPDATE seconds and decodes the messages created by the panel i/o thread.
 * It also prints the panel and keypad traffic to stdout. 
 *
 */
static void * msg_io(void * arg) {
  int cmd, res, zone, allZones[32];
  int data0, data1, data2, data3;
  int data4, data5, data6, data7;
  long unsigned int index = 0;
  char msg[50] = "";
  char word[MAX_BITS] = "", wordk[MAX_BITS] = "", buf[4*128] = "";
  struct timespec t;
  struct status * sptr = (struct status *) arg;

  // detach the thread since we don't care about its return status
  res = pthread_detach(pthread_self());
  if (res) {
    perror("message i/o thread detach failed\n");
    exit(EXIT_FAILURE);
  }

  // clear zone activity marker arrays
  for (zone = 0; zone < 32; zone++) {
    allZones[zone] = 0;
  }

  strncpy(wordk, IDLE, MAX_BITS);
  clock_gettime(CLOCK_MONOTONIC, &t);
  while (1) {
    t.tv_nsec += MSG_IO_UPDATE; // thread runs every MSG_IO_UPDATE seconds
    tnorm(&t);
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

    // Get raw data from fifo. Panel and keypad data are interleaved in the fifo. 
    res = popElement1(word, MAX_BITS);
    if (!res) { // fifo is empty (res == 0)
      continue;
    } else if (res != MAX_BITS) { // fifo read error
      fprintf(stderr, "msg_io: fifo read error\n"); // record error and continue
      continue;
    } else if (res == MAX_BITS) { // fifo has valid data
      // todo : add CRC check of raw data

      cmd = decode(word, msg, allZones); // decode word from panel into a message

      // update LED and zone status information
      if (cmd == 0x05) strcpy(sptr->ledStatus, msg);
      if (cmd == 0x27) strcpy(sptr->zone1Status, msg);
      if (cmd == 0x2d) strcpy(sptr->zone2Status, msg);
      if (cmd == 0x34) strcpy(sptr->zone3Status, msg);
      if (cmd == 0x3e) strcpy(sptr->zone4Status, msg);

      // update zone sensor activity and deactivity markers
      for (zone = 0; zone < 32; zone++) {
        if (allZones[zone]) { // zone is currently active
          if (sptr->zoneAct[zone] <= sptr->zoneDeAct[zone]) { // zone was marked inactive
            sptr->zoneAct[zone] = t.tv_sec; // zone is now active, so record time
          }
        } else { // zone is currently not active
          if (sptr->zoneDeAct[zone] < sptr->zoneAct[zone]) { // zone was marked active
            sptr->zoneDeAct[zone] = t.tv_sec; // zone is now not active, so record time
          }
        }
      }

      // update zone sensor observation time
      sptr->obsTime = t.tv_sec;

      // get raw data bytes
      data0 = getBinaryData(word,0,8);  data1 = getBinaryData(word,8,8);
      data2 = getBinaryData(word,16,8); data3 = getBinaryData(word,24,8);
      data4 = getBinaryData(word,32,8); data5 = getBinaryData(word,40,8);
      data6 = getBinaryData(word,48,8); data7 = getBinaryData(word,56,8);

      snprintf(buf, sizeof(buf),
               "index:%lu,%-50s, data: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
               index++, msg, data0, data1, data2, data3, data4, data5, data6, data7);
      fputs(buf, stdout); // display message output of panel and keypad data
      fflush(stdout);
    }

  } // while

} // msg_io

/* 
 * predict thread
 * This thread runs every PREDICT_UPDATE seconds and sends sensor data to R to make a prediction.
 * It also reads the prediction from R and does something if true.
 *
 */
static void * predict(void * arg) {
  int res, rLogFp;
  char rout[ROUT_MAX], tsBuf[sizeof("2016-05-22T12:15:22Z")];
  char popenCmd[PCMD_BUF_SIZE];
  char obsTimeBuf[RARG_SIZE] = "", zoneBuf[RARG_SIZE] = "", oldZoneBuf[RARG_SIZE] = "";
  const char * format = " %lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,"
                        "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,"
                        "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,"
                        "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu";
  struct timespec t;
  struct status * sptr = (struct status *) arg;
  struct tm *tmp;
  time_t tstamp;
  FILE * fp;
  
  // detach the thread since we don't care about its return status
  res = pthread_detach(pthread_self());
  if (res) {
    perror("message i/o thread detach failed\n");
    exit(EXIT_FAILURE);
  }

  clock_gettime(CLOCK_MONOTONIC, &t);
  while (1) {
    t.tv_nsec += PREDICT_UPDATE; // thread runs every PREDICT_UPDATE seconds
    tnorm(&t);
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

    // Time and date stamp observation, rounded to nearest second
    tstamp = time(NULL);
    tmp = gmtime(&tstamp); // Coordinated Universal Time (UTC) aka GMT timezone
    if (tmp == NULL) {
      perror("gmtime failed\n");
      exit(EXIT_FAILURE);
    }
    if (!strftime(tsBuf, sizeof(tsBuf), "%FT%TZ", tmp)) {
      fprintf(stderr, "strftime returned 0\n");
      exit(EXIT_FAILURE);
    }

    // Build strings from observation data for Rscript arguments
    snprintf(obsTimeBuf, sizeof(obsTimeBuf), " %lu", sptr->obsTime);
    snprintf(zoneBuf, sizeof(zoneBuf), format,
             sptr->zoneAct[0],  sptr->zoneAct[1],  sptr->zoneAct[2],  sptr->zoneAct[3],
             sptr->zoneAct[4],  sptr->zoneAct[5],  sptr->zoneAct[6],  sptr->zoneAct[7],
             sptr->zoneAct[8],  sptr->zoneAct[9],  sptr->zoneAct[10], sptr->zoneAct[11],
             sptr->zoneAct[12], sptr->zoneAct[13], sptr->zoneAct[14], sptr->zoneAct[15],
             sptr->zoneAct[16], sptr->zoneAct[17], sptr->zoneAct[18], sptr->zoneAct[19],
             sptr->zoneAct[20], sptr->zoneAct[21], sptr->zoneAct[22], sptr->zoneAct[23],
             sptr->zoneAct[24], sptr->zoneAct[25], sptr->zoneAct[26], sptr->zoneAct[27],
             sptr->zoneAct[28], sptr->zoneAct[29], sptr->zoneAct[30], sptr->zoneAct[31],
             sptr->zoneDeAct[0],  sptr->zoneDeAct[1],  sptr->zoneDeAct[2],  sptr->zoneDeAct[3],
             sptr->zoneDeAct[4],  sptr->zoneDeAct[5],  sptr->zoneDeAct[6],  sptr->zoneDeAct[7],
             sptr->zoneDeAct[8],  sptr->zoneDeAct[9],  sptr->zoneDeAct[10], sptr->zoneDeAct[11],
             sptr->zoneDeAct[12], sptr->zoneDeAct[13], sptr->zoneDeAct[14], sptr->zoneDeAct[15],
             sptr->zoneDeAct[16], sptr->zoneDeAct[17], sptr->zoneDeAct[18], sptr->zoneDeAct[19],
             sptr->zoneDeAct[20], sptr->zoneDeAct[21], sptr->zoneDeAct[22], sptr->zoneDeAct[23],
             sptr->zoneDeAct[24], sptr->zoneDeAct[25], sptr->zoneDeAct[26], sptr->zoneDeAct[27],
             sptr->zoneDeAct[28], sptr->zoneDeAct[29], sptr->zoneDeAct[30], sptr->zoneDeAct[31]);

    if (strcmp(zoneBuf, oldZoneBuf)) { // only run on zone changes
      /* Open the R log file for writing. If it exists, append to it; 
         otherwise, create a new file.  */ 
      rLogFp = open(RLOGPATH, O_WRONLY | O_CREAT | O_APPEND, 0666);
      if (rLogFp == -1) {
        perror ("R log open() failed\n");
        continue;
      }

      // Build and execute command to run Rscript
      snprintf(popenCmd, PCMD_BUF_SIZE, POPEN_FMT, tsBuf, obsTimeBuf, zoneBuf);
      fp = popen(popenCmd, "r");
      if (fp == NULL) {
        fprintf(stderr, "popen() failed\n");
        continue;
      }
  
      // Read output of Rscript until EOF, log and act on R's predictions
      while (fgets(rout, ROUT_MAX, fp) != NULL) {
        res = write(rLogFp, rout, strlen(rout));
        if (res != strlen(rout)) {
          perror("R log write() failed\n");
          continue;
        }

        fprintf(stdout, "%s", rout);

        if (strstr(rout, "prediction 1:  1") != NULL) {
          //fprintf(stdout, "*** R *** prediction 1 is FALSE\n");
        } else if (strstr(rout, "prediction 1:  2") != NULL) {
          //fprintf(stdout, "*** R *** prediction 1 is TRUE\n");
          system("/home/pi/bin/wemo.sh 192.168.1.105 ON > /dev/null");
        } else if (strstr(rout, "prediction 2:  1") != NULL) {
          //fprintf(stdout, "*** R *** prediction 2 is FALSE\n");
        } else if (strstr(rout, "prediction 2:  2") != NULL) {
          //fprintf(stdout, "*** R *** prediction 2 is TRUE\n");
          system("/home/pi/bin/wemo.sh 192.168.1.105 ON > /dev/null");
        } else if (strstr(rout, "prediction 3:  1") != NULL) {
          //fprintf(stdout, "*** R *** prediction 3 is FALSE\n");
        } else if (strstr(rout, "prediction 3:  2") != NULL) {
          //fprintf(stdout, "*** R *** prediction 3 is TRUE\n");
          system("/home/pi/bin/wemo.sh 192.168.1.105 ON > /dev/null");
        } else if (strstr(rout, "prediction 4:  1") != NULL) {
          //fprintf(stdout, "*** R *** prediction 4 is FALSE\n");
        } else if (strstr(rout, "prediction 4:  2") != NULL) {
          //fprintf(stdout, "*** R *** prediction 5 is TRUE\n");
          system("/home/pi/bin/wemo.sh 192.168.1.105 ON > /dev/null");
        } else if (strstr(rout, "prediction 5:  1") != NULL) {
          //fprintf(stdout, "*** R *** prediction 5 is FALSE\n");
        } else if (strstr(rout, "prediction 5:  2") != NULL) {
          //fprintf(stdout, "*** R *** prediction 5 is TRUE\n");
          system("/home/pi/bin/wemo.sh 192.168.1.105 ON > /dev/null");
        } else if (strstr(rout, "prediction 6:  1") != NULL) {
          //fprintf(stdout, "*** R *** prediction 6 is FALSE\n");
        } else if (strstr(rout, "prediction 6:  2") != NULL) {
          //fprintf(stdout, "*** R *** prediction 6 is TRUE\n");
          system("/home/pi/bin/wemo.sh 192.168.1.105 ON > /dev/null");
        } else if (strstr(rout, "prediction 7:  1") != NULL) {
          //fprintf(stdout, "*** R *** prediction 7 is FALSE\n");
        } else if (strstr(rout, "prediction 7:  2") != NULL) {
          //fprintf(stdout, "*** R *** prediction 7 is TRUE\n");
          system("/home/pi/bin/wemo.sh 192.168.1.105 ON > /dev/null");
        } else if (strstr(rout, "prediction 8:  1") != NULL) {
          //fprintf(stdout, "*** R *** prediction 8 is FALSE\n");
        } else if (strstr(rout, "prediction 8:  2") != NULL) {
          //fprintf(stdout, "*** R *** prediction 8 is TRUE\n");
          system("/home/pi/bin/wemo.sh 192.168.1.105 ON > /dev/null");
        }
      }

      res = close(rLogFp); 
      if (res == -1) {
        perror ("R log close() failed\n");
        exit(EXIT_FAILURE);
      }

      res = pclose(fp);
      if (res == -1) {
        perror("pclose() failed\n");
        exit(EXIT_FAILURE);
      }
      
    }

    strcpy(oldZoneBuf, zoneBuf);

  } // while

} // predict

// server
static int create_socket(int port)
{
  int listenfd = 0, res;
  struct sockaddr_in server_addr;
  
  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd == -1) {
    perror("server: could not open socket\n");
    exit(EXIT_FAILURE);;
  }

  memset(&server_addr, 0, sizeof(server_addr)); 
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(port); 
  res = bind(listenfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
  if (res) {
    perror("server: bind() failed\n");
    exit(EXIT_FAILURE);;
  }

  res = listen(listenfd, BACKLOG);
  if (res == -1) {
    perror("server: listen() failed\n");
    exit(EXIT_FAILURE);;
  }

  return listenfd;
} // create_socket()

// openssl - reference: https://wiki.openssl.org/index.php/Simple_TLS_Server

static void init_openssl() {
  SSL_library_init();
  OpenSSL_add_all_algorithms(); 
  SSL_load_error_strings();	
} // openssl()

static void cleanup_openssl() {
  EVP_cleanup();
} // cleanup_openssl()

static SSL_CTX *create_context() {
  const SSL_METHOD *method;
  SSL_CTX *ctx;

  method = SSLv23_server_method();

  ctx = SSL_CTX_new(method);
  if (!ctx) {
    perror("Unable to create SSL context");
    ERR_print_errors_fp(stderr);
    exit(EXIT_FAILURE);
  }

  return ctx;
} // *create_context()

static void configure_context(SSL_CTX *ctx) {
  //SSL_CTX_set_ecdh_auto(ctx, 1); // supported from openssl 1.0.2, using 1.0.1e

  /*
   * certs and key pairs generated by the steps below
   *
   * Step 1. Generate ca private key
   * $ openssl genrsa -out /home/pi/certs/ca/ca.key 4096
   * Step 2. Create self-signed ca cert, COMMON_NAME="My CA"
   * $ openssl req -new -x509 -days 365 -key /home/pi/certs/ca/ca.key \
   *           -out /home/pi/certs/ca/ca.crt -sha256
   *
   * Step 3. Create client private key
   * $ openssl genrsa -out /home/pi/certs/client/client.key 2048
   * Step 4. Create client cert signing request, COMMON_NAME="Client 1"
   * $ openssl req -new -key /home/pi/certs/client/client.key \
   *           -out /home/pi/certs/client/client.csr -sha256
   * Step 5. Create signed client cert
   * $ openssl x509 -req -days 365 -in /home/pi/certs/client/client.csr -CA /home/pi/certs/ca/ca.crt \
   *           -CAkey /home/pi/certs/ca/ca.key -set_serial 01 \
   *           -out /home/pi/certs/client/client.crt -sha256
   *
   * Step 6. Create server private key
   * $ openssl genrsa -out /home/pi/certs/server/server.key 2048
   * Step 7. Create server cert signing request, COMMON_NAME="localhost"
   * $ openssl req -new -key /home/pi/certs/server/server.key \
   *           -out /home/pi/certs/server/server.csr -sha256
   * Step 8. Create signed server cert, where "key.ext" contains "subjectAltName = IP:xxx.xxx.xxx.xxx"
   * $ openssl x509 -req -days 365 -in /home/pi/certs/server/server.csr -CA /home/pi/certs/ca/ca.crt \
   *           -CAkey /home/pi/certs/ca/ca.key -set_serial 02 \
   *           -out /home/pi/certs/server/server.crt -sha256 -extfile /home/pi/certs/server/key.ext
   *
   * Step 9. Copy client key pair and CA certificate to Lambda
   * $ cp /home/pi/certs/client/client.crt /home/pi/all/lambda
   * $ cp /home/pi/certs/client/client.key /home/pi/all/lambda
   * $ cp /home/pi/certs/ca/ca.crt /home/pi/all/lambda
   */

  if (SSL_CTX_use_certificate_file(ctx, "/home/pi/certs/server/server.crt", SSL_FILETYPE_PEM) < 0) {
    ERR_print_errors_fp(stderr);
    exit(EXIT_FAILURE);
  }

  if (SSL_CTX_use_PrivateKey_file(ctx, "/home/pi/certs/server/server.key", SSL_FILETYPE_PEM) < 0 ) {
    ERR_print_errors_fp(stderr);
    exit(EXIT_FAILURE);
  }

  // check if the certificate and private key match
  if (SSL_CTX_check_private_key(ctx) != 1) {
    fprintf(stderr, "Private key does not match the certificate public key\n");
    exit(EXIT_FAILURE);
  }

  // SSL_VERIFY_PEER is set so server sends a client certificate request.
  SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

  // set the list of trusted CAs
  if (SSL_CTX_load_verify_locations(ctx, "/home/pi/certs/ca/ca.crt", "/home/pi/certs/ca") < 1) {
    fprintf(stderr, "Error setting the verify locations.\n");
    exit(EXIT_FAILURE);
  }
} // configure_context()

static void panserv(struct status * pstat, int port) {
  char buffer[BUF_LEN]="", wordk[MAX_BITS] = "";
  char txBuf[1024];
  char addrStr[ADDRSTRLEN];
  char host[NI_MAXHOST];
  char service[NI_MAXSERV];
  const char *jsonObj = "{\"obsTime\":%lu,"
                        "\"zoneAct\":["
                        "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,"
                        "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu],"
                        "\"zoneDeAct\":["
                        "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,"
                        "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu]}\n";
  int listenfd= 0, connfd = 0, res, num, i, tag = 0;
  long chkbuf;
  socklen_t addrlen;  
  struct sockaddr_in client_addr;
  SSL_CTX *ctx;
  SSL *ssl;

  init_openssl();
  ctx = create_context();
  configure_context(ctx);

  listenfd = create_socket(port);

  signal(SIGPIPE, SIG_IGN); // receive EPIPE from a failed write()

  for (;;) {
    i = 0;
    memset(&client_addr, 0, sizeof(client_addr));
    addrlen = sizeof(struct sockaddr_storage);

    connfd = accept(listenfd, (struct sockaddr *) &client_addr, &addrlen);
    if (connfd == -1) {
      perror("server: accept failed\n");
      continue;
    }

    if (!getnameinfo ((struct sockaddr *) &client_addr, addrlen,
        host, NI_MAXHOST, service, NI_MAXSERV, 0))
      snprintf(addrStr, ADDRSTRLEN, "(%s, %s)", host, service);
    else
      snprintf(addrStr, ADDRSTRLEN, "(?UNKNOWN?)");
    fprintf(stdout, "server: connection requested from %s\n", addrStr);

    if (!hosts_ctl("kprw-server", STRING_UNKNOWN, host, STRING_UNKNOWN)) {
      fprintf(stderr, "Client %s connection disallowed\n", inet_ntoa(client_addr.sin_addr));
      close(connfd);
      continue;
    }

    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, connfd);

    if (SSL_accept(ssl) <= 0) {
      ERR_print_errors_fp(stderr);
      SSL_free(ssl);
      close(connfd);
      continue;
    }

    fprintf(stdout, "server: client %s connected with %s encryption\n",
            inet_ntoa(client_addr.sin_addr), SSL_get_cipher(ssl));

    memset(&buffer, 0, BUF_LEN);
    res = SSL_read(ssl, buffer, (BUF_LEN-1)); // read command from socket
    if (res <= 0) {
      ERR_print_errors_fp(stderr);
      SSL_free(ssl);
      close(connfd);
      continue;
    }

    fprintf(stdout, "server: panel received command %s", buffer);

    /*
     * Decode and process a command sent from client.
     * Check for bad commands.
     * Map to keypad data and send to panel.
     *
     * todo: this should be a function
     */
    // process commands
    if (!isdigit(buffer[i])) { // not a number, but a command
      if (!strncmp(buffer, "star", 4))
        strncpy(wordk, STAR, MAX_BITS);
      else if (!strncmp(buffer, "pound", 5))
        strncpy(wordk, POUND, MAX_BITS);
      else if (!strncmp(buffer, "stay", 4))
        strncpy(wordk, STAY, MAX_BITS);
      else if (!strncmp(buffer, "away", 4))
        strncpy(wordk, AWAY, MAX_BITS);
      else if (!strncmp(buffer, "idle", 4))
        strncpy(wordk, IDLE, MAX_BITS);
      else if (!strncmp(buffer, "tag", 3)) {
        strncpy(wordk, IDLE, MAX_BITS);
        tag = 1;
      } else {
        fprintf(stderr, "server: invalid panel command\n");
        strncpy(wordk, IDLE, MAX_BITS);
      }
      // send keypad data to panel
      res = pushElement2(wordk, MAX_BITS);
      if (res != MAX_BITS) {
        fprintf(stderr, "server: fifo write error\n");
        break;
      }
    } else { // a number or number(s)
      chkbuf = strtol(buffer, NULL, 10);
      if (chkbuf < 0 || chkbuf > 9999) {
        fprintf(stderr, "server: invalid panel command\n");
        continue;
      }
      while(buffer[i] != '\n') {
        num = buffer[i] - '0';
        switch (num) {
          case 0 :
            strncpy(wordk, ZERO, MAX_BITS);
            break;
          case 1 :
            strncpy(wordk, ONE, MAX_BITS);
            break;
          case 2 :
            strncpy(wordk, TWO, MAX_BITS);
            break;
          case 3 :
            strncpy(wordk, THREE, MAX_BITS);
            break;
          case 4 :
            strncpy(wordk, FOUR, MAX_BITS);
            break;
          case 5 :
            strncpy(wordk, FIVE, MAX_BITS);
            break;
          case 6 :
            strncpy(wordk, SIX, MAX_BITS);
            break;
          case 7 :
            strncpy(wordk, SEVEN, MAX_BITS);
            break;
          case 8 :
            strncpy(wordk, EIGHT, MAX_BITS);
            break;
          case 9 :
            strncpy(wordk, NINE, MAX_BITS);
            break;
          default :
            fprintf(stderr, "server: invalid panel command\n");
            strncpy(wordk, IDLE, MAX_BITS);
        }
        // send keypad data to panel
        res = pushElement2(wordk, MAX_BITS);
        if (res != MAX_BITS) {
          fprintf(stderr, "server: fifo write error\n");
          break;
        }
        i++;
        if (i > 4) { // max 4-digit number allowed
          fprintf(stderr, "server: invalid panel command\n");
          continue;
        }
      }
    }
    
    // send back zone and system status, either as JSON or text
    if (tag) { // send zone data as JSON
      snprintf(txBuf, sizeof(txBuf), jsonObj,
               pstat->obsTime,
               pstat->zoneAct[0],  pstat->zoneAct[1],  pstat->zoneAct[2],  pstat->zoneAct[3],
               pstat->zoneAct[4],  pstat->zoneAct[5],  pstat->zoneAct[6],  pstat->zoneAct[7],
               pstat->zoneAct[8],  pstat->zoneAct[9],  pstat->zoneAct[10], pstat->zoneAct[11],
               pstat->zoneAct[12], pstat->zoneAct[13], pstat->zoneAct[14], pstat->zoneAct[15],
               pstat->zoneAct[16], pstat->zoneAct[17], pstat->zoneAct[18], pstat->zoneAct[19],
               pstat->zoneAct[20], pstat->zoneAct[21], pstat->zoneAct[22], pstat->zoneAct[23],
               pstat->zoneAct[24], pstat->zoneAct[25], pstat->zoneAct[26], pstat->zoneAct[27],
               pstat->zoneAct[28], pstat->zoneAct[29], pstat->zoneAct[30], pstat->zoneAct[31],
               pstat->zoneDeAct[0],  pstat->zoneDeAct[1],  pstat->zoneDeAct[2],  pstat->zoneDeAct[3],
               pstat->zoneDeAct[4],  pstat->zoneDeAct[5],  pstat->zoneDeAct[6],  pstat->zoneDeAct[7],
               pstat->zoneDeAct[8],  pstat->zoneDeAct[9],  pstat->zoneDeAct[10], pstat->zoneDeAct[11],
               pstat->zoneDeAct[12], pstat->zoneDeAct[13], pstat->zoneDeAct[14], pstat->zoneDeAct[15],
               pstat->zoneDeAct[16], pstat->zoneDeAct[17], pstat->zoneDeAct[18], pstat->zoneDeAct[19],
               pstat->zoneDeAct[20], pstat->zoneDeAct[21], pstat->zoneDeAct[22], pstat->zoneDeAct[23],
               pstat->zoneDeAct[24], pstat->zoneDeAct[25], pstat->zoneDeAct[26], pstat->zoneDeAct[27],
               pstat->zoneDeAct[28], pstat->zoneDeAct[29], pstat->zoneDeAct[30], pstat->zoneDeAct[31]);

      res = SSL_write(ssl, txBuf, strlen(txBuf)); // write json to socket
      if (res <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(connfd);
        continue;
      }
      
      tag = 0;
    } else { // send zone data as text, this is the default format
      snprintf(txBuf, sizeof(txBuf), "%s, %s, %s, %s, %s,",
               pstat->ledStatus, pstat->zone1Status, pstat->zone2Status,
               pstat->zone3Status, pstat->zone4Status);

      res = SSL_write(ssl, txBuf, strlen(txBuf)); // write status to socket
      if (res <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(connfd);
        continue;
      }
    }

    SSL_free(ssl);
    res = close(connfd);
    if (res == -1) {
      perror("server: error closing connection");
      continue;
    }   

  }

  close(listenfd);
  SSL_CTX_free(ctx);
  cleanup_openssl();

  return;
} // panserv

int main(int argc, char *argv[])
{
  int res, crit1, crit2, flag, i, port;
  struct sched_param param_main, param_pio, param_predict;
  struct utsname u;
  struct status pstat;
  pthread_t pio_thread, mio_thread, main_thread, predict_thread;
  pthread_attr_t my_attr;
  cpu_set_t cpuset_mio, cpuset_pio, cpuset_main;
  FILE *fd;

  // Check program args and get server port number.
  if (argc != 2) {
    fprintf(stderr, "usage: %s port<49152–65535>\n", argv[0]);
    exit(EXIT_FAILURE);
  } else {
    port = strtol(argv[1], NULL, 10);
    if (port < 49152 || port > 65535) {
      fprintf(stderr, "Port number must be in the range of 49152 to 65535\n");
      exit(EXIT_FAILURE);
    }
  }

  // Check if running with real-time linux.
  uname(&u);
  crit1 = (int) strcasestr (u.version, "PREEMPT RT");
  if ((fd = fopen("/sys/kernel/realtime","r")) != NULL) {
    crit2 = ((fscanf(fd, "%d", &flag) == 1) && (flag == 1));
    fclose(fd);
  }
  fprintf(stdout, "This is a %s kernel.\n", (crit1 && crit2)  ? "PREEMPT RT" : "vanilla");
  if (!(crit1 && crit2)) {
    fprintf(stderr, "Can't run under a vanilla kernel\n");
    exit(EXIT_FAILURE);
  }

  // todo: add checks if running with su priv and raspberry pi 2 hw. 

  // CPU(s) for main, predict and message i/o threads
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

  // Declare ourself as a real time task
  param_main.sched_priority = MAIN_PRI;
  if(sched_setscheduler(0, SCHED_FIFO, &param_main) == -1) {
    perror("sched_setscheduler failed\n");
    exit(EXIT_FAILURE);
  }

  // set main thread cpu affinity
  main_thread = pthread_self();
  res = pthread_setaffinity_np(main_thread, sizeof(cpuset_main), &cpuset_main);
  if (res) {
    perror("Main thread cpu affinity set failed\n");
    exit(EXIT_FAILURE);
  }

  // Lock memory to prevent page faults
  if(mlockall(MCL_CURRENT|MCL_FUTURE) == -1) {
    perror("mlockall failed\n");
    exit(EXIT_FAILURE);
  }

  // Turn off malloc trimming.
  mallopt(M_TRIM_THRESHOLD, -1);
   
  // Turn off mmap usage.
  mallopt(M_MMAP_MAX, 0);

  // Pre-fault our stack and display page fault stats
  reserve_process_memory(MAX_SAFE_STACK);
  show_new_pagefault_count("malloc() and touch generated", ">=0", ">=0");

  // test to see if pre-faulting worked
  reserve_process_memory(MAX_SAFE_STACK);
  show_new_pagefault_count("2nd malloc() and use generated", "0", "0");

  // Set up gpio pointer for direct register access
  setup_io();

  // Set up FIFOs
  m_Read1 = 0;
  m_Read2 = 0;
  m_Write1 = 0;
  m_Write2 = 0;
  for (i = 0; i < MAX_DATA; i++) {
    m_Data1[i] = '0';
    m_Data2[i] = '0';
  }

  // init panel status indicators
  memset(&pstat, 0, sizeof(pstat));

  // Set pin direction
  INP_GPIO(PI_DATA_OUT); // must use INP_GPIO before we can use OUT_GPIO
  OUT_GPIO(PI_DATA_OUT);
  INP_GPIO(PI_DATA_IN);
  INP_GPIO(PI_CLOCK_IN);

  // Set PI_DATA_OUT pin low.
  GPIO_CLR = 1<<PI_DATA_OUT;

  // create panel input / output thread
  pthread_attr_init(&my_attr);
  pthread_attr_setaffinity_np(&my_attr, sizeof(cpuset_pio), &cpuset_pio);
  pthread_attr_setschedpolicy(&my_attr, SCHED_FIFO);
  // Set the requested stacksize for this thread
  res = pthread_attr_setstacksize(&my_attr, PTHREAD_STACK_MIN + MY_STACK_SIZE);
  if (res) {
    perror("Panel i/o thread set stack size failed\n");
    exit(EXIT_FAILURE);
  }
  param_pio.sched_priority = PANEL_IO_PRI;
  pthread_attr_setschedparam(&my_attr, &param_pio);
  res = pthread_create(&pio_thread, &my_attr, panel_io, NULL);
  if (res) {
    perror("Panel i/o thread creation failed\n");
    exit(EXIT_FAILURE);
  }
  pthread_attr_destroy(&my_attr);

  // create message input / output thread
  pthread_attr_init(&my_attr);
  pthread_attr_setaffinity_np(&my_attr, sizeof(cpuset_mio), &cpuset_mio);
  pthread_attr_setschedpolicy(&my_attr, SCHED_FIFO);
  // Set the requested stacksize for this thread
  res = pthread_attr_setstacksize(&my_attr, PTHREAD_STACK_MIN + MY_STACK_SIZE);
  if (res) {
    perror("Message i/o thread set stack size failed\n");
    exit(EXIT_FAILURE);
  }
  param_pio.sched_priority = MSG_IO_PRI;
  pthread_attr_setschedparam(&my_attr, &param_pio);
  res = pthread_create(&mio_thread, &my_attr, msg_io, (void *) &pstat);
  if (res) {
    perror("Message i/o thread creation failed\n");
    exit(EXIT_FAILURE);
  }
  pthread_attr_destroy(&my_attr);

  // create predict thread
  pthread_attr_init(&my_attr);
  pthread_attr_setaffinity_np(&my_attr, sizeof(cpuset_mio), &cpuset_mio);
  pthread_attr_setschedpolicy(&my_attr, SCHED_FIFO);
  // Set the requested stacksize for this thread
  res = pthread_attr_setstacksize(&my_attr, PTHREAD_STACK_MIN + MY_STACK_SIZE);
  if (res) {
    perror("Predict thread set stack size failed\n");
    exit(EXIT_FAILURE);
  }
  param_predict.sched_priority = PREDICT_PRI;
  pthread_attr_setschedparam(&my_attr, &param_predict);
  res = pthread_create(&predict_thread, &my_attr, predict, (void *) &pstat);
  if (res) {
    perror("Predict thread creation failed\n");
    exit(EXIT_FAILURE);
  }
  pthread_attr_destroy(&my_attr);

  // start server
  panserv(&pstat, port);

  // cleanup - unlock memory
  if(munlockall() == -1) {
    perror("munlockall failed\n");
    exit(EXIT_FAILURE);
  }

  return(0);
} // main
