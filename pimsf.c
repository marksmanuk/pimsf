/******************************************************************************
  60kHz MSF Time Signal Transmitter
  by Mark Street <marksmanuk@gmail.com>

  Version 1.00 December 2017

  Connect antenna to GPIO4 pin 7
******************************************************************************/

#pragma GCC diagnostic ignored "-Wstrict-aliasing"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

int verbose = 0;
const static int TIME_WAITMIN = 3330;
const static int TIME_SKEWTX  = -13;

int mem_fd;
char *gpio_mem, *gpio_map;
char *spi0_mem, *spi0_map;

// I/O access
volatile unsigned *gpio;
volatile unsigned *allof7e;

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET *(gpio+7)	// sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10)	// clears bits which are 1 ignores bits which are 0
#define GPIO_GET *(gpio+13)	// sets   bits which are 1 ignores bits which are 0

#define ACCESS(base) *(volatile int*)((int)allof7e+base-0x7e000000)
#define SETBIT(base, bit) ACCESS(base) |= 1<<bit
#define CLRBIT(base, bit) ACCESS(base) &= ~(1<<bit)

#define CM_GP0CTL (0x7e101070)
#define GPFSEL0 (0x7E200000)
#define CM_GP0DIV (0x7e101074)
#define CLKBASE (0x7E101000)
#define DMABASE (0x7E007000)
#define PWMBASE  (0x7e20C000) /* PWM controller */

struct GPCTL {
    char SRC         : 4;
    char ENAB        : 1;
    char KILL        : 1;
    char             : 1;
    char BUSY        : 1;
    char FLIP        : 1;
    char MASH        : 2;
    unsigned int     : 13;
    char PASSWD      : 8;
};

struct MSF {
	int a;
	int b;	
};

void nsleep(unsigned long int period)
{
	struct timespec ts;
	ts.tv_sec = period / 1000000;
	ts.tv_nsec = (period % 1000000) * 1000;
	while (nanosleep(&ts, &ts) && errno == EINTR);
}

void setup_gpclk0(int mash, int divi, int divf)
{
    /* Open /dev/mem */
    if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC)) < 0)
	{
        printf("Failed to open /dev/mem\n");
        exit(-1);
    }
    
    allof7e = (unsigned *)mmap(
                  NULL,
                  0x01000000,  //len
                  PROT_READ|PROT_WRITE,
                  MAP_SHARED,
                  mem_fd,
                  0x20000000  //base
              );

    if ((int)allof7e == -1)
		exit(-1);

	// FSEL4 alternate function 0 = GPCLK0
    SETBIT(GPFSEL0, 14);
    CLRBIT(GPFSEL0, 13);
    CLRBIT(GPFSEL0, 12);

	// Condigure clock control register, leave disabled
    struct GPCTL setupword = {
		1,		/* clock source */
	   	0,		/* enable */
	   	0,		/* not kill */
	   	0, 0,
		(char)mash,	/* MASH */
		0x5a	/* password */
	};

    ACCESS(CM_GP0CTL) = *((int*)&setupword);
	nsleep(100);

	// Setup DIV:
	printf("Setting up GPCLK0 MASH=%d DIVI=%d DIVF=%d\n", mash, divi, divf);
	int ua = (0x5a << 24) + (divi << 12) + divf;
	ACCESS(CM_GP0DIV) = ua;
	nsleep(100);

	// Enable clock:
	int ctl = ACCESS(CM_GP0CTL) | 0x5a000000;
	ctl |= (1 << 4);
    ACCESS(CM_GP0CTL) = *((int*)&ctl);
	nsleep(100);
}

void clock_startstop(int state)
{
	int ctl = ACCESS(CM_GP0CTL) | 0x5a000000;

	if (state)
		ctl |= (1 << 4);
	else
		ctl &= ~(1 << 4);

    ACCESS(CM_GP0CTL) = *((int*)&ctl);
}

void encode_timecode(struct MSF *msf, time_t t_now)
{
	int bcd[] = { 80, 40, 20, 10, 8, 4, 2, 1 };
	memset(msf, 0, 60 * sizeof(struct MSF));	// Zero all values

	t_now += 60;	// Encode for following minute
	struct tm *tm_now = localtime(&t_now);
	if (verbose)
		printf("%s() %s", __func__, asctime(tm_now));

	// Year 17-24
    int temp = tm_now->tm_year - 100;	// Year from 1900
    int sum = 0, bcdindex = 0;
    for (int i=17; i<=24; i++)
	{
        if (temp >= bcd[bcdindex])
		{
            msf[i].a = 1;
            sum++;
            temp -= bcd[bcdindex];
		}
        bcdindex++;
	}

    if (!(sum % 2))		// 17A-24A
        msf[54].b = 1;

	// Month 25-29
    temp = tm_now->tm_mon + 1;	// tm_mon 0-11
	bcdindex = 3;	// 10
    sum = 0;
    for (int i=25; i<=29; i++)
	{
        if (temp >= bcd[bcdindex])
		{
            msf[i].a = 1;
            sum++;
            temp -= bcd[bcdindex];
		}
        bcdindex++;
	}

	// Day 30-35
    temp = tm_now->tm_mday;	// tm_day 1-31
	bcdindex = 2;	// 20
    for (int i=30; i<=35; i++)
	{
        if (temp >= bcd[bcdindex])
		{
            msf[i].a = 1;
            sum++;
            temp -= bcd[bcdindex];
		}
        bcdindex++;
	}

    if (!(sum % 2))		// 25A-35A
        msf[55].b = 1;

	// Day of Week 36-38
    temp = tm_now->tm_wday;	// tm_wday 0-6 Sunday
	bcdindex = 5;	// 04
    sum = 0;
    for (int i=36; i<=38; i++)
	{
        if (temp >= bcd[bcdindex])
		{
            msf[i].a = 1;
            sum++;
            temp -= bcd[bcdindex];
		}
        bcdindex++;
	}

	if (!(sum % 2))		// 36A-38A
		msf[56].b = 1;

	// Hour 39-44
    temp = tm_now->tm_hour;	// tm_hour 0-23
	bcdindex = 2;	// 20
    sum = 0;
    for (int i=39; i<=44; i++)
	{
        if (temp >= bcd[bcdindex])
		{
            msf[i].a = 1;
            sum++;
            temp -= bcd[bcdindex];
		}
        bcdindex++;
	}

	// Minute 45-51
    temp = tm_now->tm_min;	// tm_min 0-59
	bcdindex = 1;	// 40
    for (int i=45; i<=51; i++)
	{
        if (temp >= bcd[bcdindex])
		{
            msf[i].a = 1;
            sum++;
            temp -= bcd[bcdindex];
		}
        bcdindex++;
	}

	if (!(sum % 2))		// 39A-51A
		msf[57].b = 1;

	// DST 58
	msf[58].b = tm_now->tm_isdst;

	// A bits 53-58 are always 1
	for (int i=53; i<=58; i++)
		msf[i].a = 1;
}

void key(int code, int offset=0)
{
	// printf("%s() Keying code %02X\n", __func__, code);

	// Minute marker
	if (code == 0xff)
	{
		clock_startstop(0);
		nsleep(500*1000);
		clock_startstop(1);
		nsleep((500+offset)*1000);	// Apply timing offset
	}

	// Code 00
	if (code == 0x00)
	{
		clock_startstop(0);
		nsleep(100*1000);
		clock_startstop(1);
		nsleep(900*1000);
	}

	// Code 01
	if (code == 0x01)
	{
		clock_startstop(0);
		nsleep(100*1000);
		clock_startstop(1);
		nsleep(100*1000);
		clock_startstop(0);
		nsleep(100*1000);
		clock_startstop(1);
		nsleep(700*1000);
	}

	// Code 10
	if (code == 0x10)
	{
		clock_startstop(0);
		nsleep(200*1000);
		clock_startstop(1);
		nsleep(800*1000);
	}

	// Code 11
	if (code == 0x11)
	{
		clock_startstop(0);
		nsleep(300*1000);
		clock_startstop(1);
		nsleep(700*1000);
	}
}

void send_timecode(int duration)
{
	if (duration)
		printf("Sending MSF timecode for %d seconds\n", duration);
	else
		printf("Sending MSF timecode continuously\n");

	if (duration && duration < 60)
	{
		fprintf(stderr, "Error! Minimum transmit time of 60s required\n");
		clock_startstop(0);
		exit(1);
	}

	MSF timecode[60];

	// Precision clock:
	struct tm *tm_now;
	struct timeval tv;
	char buffer[36];

	gettimeofday(&tv, NULL);
	int tv_start = tv.tv_sec;

	while (!duration || tv.tv_sec < tv_start + duration)
	{
		// Align with system clock:
		gettimeofday(&tv, NULL);
		tm_now = localtime(&tv.tv_sec);
		strftime(buffer, 36, "%A %b %d %Y %H:%M:%S", tm_now);
		printf("Timecode starting %s.%06ld\n", buffer, tv.tv_usec);

		// Align to minute boundary:
		if (tm_now->tm_sec != 0 || (tm_now->tm_sec == 0 && (tv.tv_usec/1000.0) > 250))
		{
			unsigned long int delta = 60000000 - ((tm_now->tm_sec*1000000) + tv.tv_usec);
			printf("Waiting %.3f seconds for clock alignment\n", delta/1000000.0); 
			if (delta > TIME_WAITMIN)
				nsleep(delta - TIME_WAITMIN);
			continue;
		}

		// Encode a new message for the following minute:
		encode_timecode(timecode, tv.tv_sec);

		// Apply time correction should we lag system time:
		int offset = TIME_SKEWTX - (tv.tv_usec/1000);
//		printf("Delta %ld us, Offset %d us\n", tv.tv_usec, offset);
		
		// Transmit message over 60s period:
		key(0xff, offset);
		for (int i=1; i<60; i++)
		{
			if (verbose)
				printf("  Bit: %02d   A:%d B:%d\n", i, timecode[i].a, timecode[i].b);
			key((timecode[i].a << 4) + timecode[i].b);
		}

		// Sending complete
		gettimeofday(&tv, NULL);
		if (verbose)
		{
			tm_now = localtime(&tv.tv_sec);
			strftime(buffer, 36, "%A %b %d %Y %H:%M:%S", tm_now);
			printf("Timecode finished %s.%06ld\n", buffer, tv.tv_usec);
		}
	}
}

void signal_handler(int signo)
{
	if (signo == SIGINT)
	{
		printf("\nSIGINT received, shutting down carrier\n");
		clock_startstop(0);
		exit(0);
	}
}

int main(int argc, char **argv)
{
	int duration = 0;

	if (signal(SIGINT, signal_handler) == SIG_ERR)
		printf("Error! Unable to catch SIGINT\n");

	// DIVI = 19.2MHz/60kHz = 320
	setup_gpclk0(0, 320, 0);

	int args;
	while ((args = getopt(argc, argv, "vset:")) != EOF)
	{
		switch(args)
		{
			case 'v':
				verbose = 1;
				break;
			case 's':
				clock_startstop(1);
				return 0;
				break;
			case 'e':
				clock_startstop(0);
				return 0;
				break;
			case 't':
				duration = atoi(optarg);
				break;
			default:
				fprintf(stderr, "Usage: pimsf [options]\n" \
					"\t-s Start 60kHz carrier\n" \
					"\t-e Stop 60kHz carrier\n" \
					"\t-t <duration> Send timecode for duration seconds\n" \
					"\t-v Verbose\n"
				);
				clock_startstop(0);
				return 1;
		}
	}

	send_timecode(duration);
	clock_startstop(0);
	return 0;
}

