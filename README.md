# CSC 464 - Networks - Hector Pule - 9pm (Lab)

## trace.c

For this file, I used a lot of functions and tried to reduce the amount of code I needed to write while keeping it reasonable.

## trace.h

Each stuct is packed and has its own variables. This made it easier for me to grasp and allowed me to keep my functions short by just using memcpy in places where I wanted specific pieces.

***Packed Structs:***

- ethernet_frame

- arp_header

- ip_header

- tcp_header

- pseudo_header

- udp_header

- icmp_header

The processing functions take in the next packet and its length, which they add to their respective structures and then print out the corresponding information.  Such as destination, source, length, checksum, and ports.

**Processing Functions**

- ```process_ethernet()```

- ```process_arpp()```

- ```process_icmp()```

- ```process_udp()```

- ```process_tcp()```

**Helper Functions**

- ```tcp_checksum()```

- ```get_tcp_length()```
# true_chatt
# true_chatt
