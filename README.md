# tinypot
A very simple telnet honeypot

This is a very simple telnet honeypot program. It displays a login: prompt, followed by a password: prompt. After that, it simply echos whatever inputs it receives.

## Building tinypot
Just issue this command:
```
$ gcc -pthread tinypot_main.c  tinypot_process.c -o tinypot
```

## Executing tinypot
The general invocation is
```
$ tinypot [ip_address|-] portnum portnum portnum ...
```
The first argument determines what local network interface tinypot will listen on for incoming connections. The argument is either a specific IP address, or the single character "-". In the latter case, tinypot will listen on all available network interfaces.

Each portnum is a TCP/IP port number that will be opened for listening.

To fish for Mirai bots, try this:
```
$ tinypot - 23 2323 23231 6789
```

## Comments
This program provides a simple example of a TCP/IP server program, using BSD socket functions. It is also a simple example of Posix thread programming.

You can use this program to watch telnet malware at work. In particular, Mirai. For this purpose, any or all of the ports 23, 2323, 23231, or 6789 may be used. These are the ports that are used by Mirai and similar malware.

The malware programs vary in their sophistication. Some of them will detect the fact that tinypot is not a vulnerable telnet server and disconnect immediately. Other malware programs will send several commands before giving up. It can be interesting to watch. You can expect to wait anywhere from one to 25 minutes between connection attempts.
