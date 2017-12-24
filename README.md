# PiMSF - 60kHz MSF Time Signal for Raspberry Pi

This is a small program for emulating the MSF Radio Time Signal using a Raspberry Pi.

Connect a length of wire to GPIO4 Pin 7 to serve as an antenna.

Written to time synchronise a Casio digital watch which supports synchronisation using the UK based MS radio time signal.

## Getting Started

Download the latest release of this program and copy the files to your Raspberry Pi.  The code was developed and tested using Raspbian Stretch Lite on a version 1B Raspberry Pi.

I've included a pre-compiled 'pimsf' executable which you can copy to your Pi and use straight away.

To compile your own version, copy the Makefile and pimsf.c to the Pi.  You will need gcc installed.

### Compiling

```
$ make
```

### Usage

To start the program, enter:

```
$ sudo ./pimsf
```

For help enter:

```
$ sudo ./pimsf -h
Usage: pimsf [options]
        -s Start 60kHz carrier
        -e Stop 60kHz carrier
        -t <duration> Send timecode for duration seconds
        -v Verbose
```

The -t switch limits the transmit time to the period specified, in seconds.  For example, Casio watches only listen for the time signal at set intervals each day, and once received successfully will not make further attempts until the next day.  Therefore, a cron job on the Pi could be used to send the time signal at the times the watch is known to be listening.

With no switches specified the time signal is sent indefinitely.

### Reception

The effective range is limited to about 1 or 2 feet using a short antenna connected to GPIO4.  For adequate results make a loop of several turns of wire and place the watch inside or close to it.

The Casio PRW-1300Y module number 3070 synchronises consistently every day on the 1st attempt when set to LON (London) time zone.

The Casio sync behaviour is quite interesting:
Initially there is zero signal level shown for about 10 seconds, it can then show 1 bar of signal for a further 5 to 10s, then it shows full strength 3 bars.  It takes a further 1 minute cycle to receive the time signal.  In practice this means it needs at least 2 mins to receive the time code.  If syncing manually the watch beeps and shows "GET" along with the time & date of last reception if successful. 

In timekeeping mode, if the signal level meter shows 3 bars it means the watch has received a successful time code during the current day.  Strangely I've never seen a signal level other than 'flashing 1 bar' and '3 bars' although the manual implies there's a scale of 1 to 5 values.

### Accuracy

The program sends the system date & time from the Raspberry Pi.  I've assumed the Pi is running NTP and synchronised to a suitable NTP time server over the internet.  In testing I've been using the Chrony replacement NTP daemon.

### Prerequisites

* Raspberry Pi
* gcc - to complile the program

## Authors

* **Mark Street** [marksmanuk](https://github.com/marksmanuk)

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details

