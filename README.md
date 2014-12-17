subnettools
===========

subnet file tool !

a tool designed to help network engineer manipulate large route/subnet CSV files
FEATURES
========
- compare CSV files (common routes, missing routes)
- get a subnet file declaration from an IPAM file (IPAM format fully dynamic)
- grep files for subnets
- simplifying CSV route files (removing duplicates, sorting)
- aggregating subnets as much as possible
- converting 'sh ip route' files to a CSV

- subnettools main format is CSV; delimitors, name of the fields describing (prefix, mask, gw, dev, comment) are fully configurable
- subnettools output format is configurable ; you can configure FMT (%I %m %D %G %C)
- subnettools supports IPv4 and IPv6
- subnettools has a default config file (st.conf)
- subnettools has a debug system (subnettools -D help), mostly helped me to create it
- subnettools has a small regression testing suite


LIMITS
=======
- file size : more than enough (aggretated a 8millions line CSV in 2-3minutes)
- line size : 1024 (subnettools truncates too long lines)
- comment size : 128 bytes (i butcher it if too long)
- name of CSV fields : 32 bytes (OK unless you use stupids name to describe a prefix, mask, comment... french longest word is 26 bytes anyway, anticonstitutionnellement)
- max number of delimiters : 30 (why would you want more?)
- device name : 32 bytes (should be enough, but could be increased, TenGigabitethernet2/2/0.4121  is 29 bytes) 

it should work OK today for most functions

CSV format 
===========
- Input subnet files SHOULD have a CSV header describing its structure (prefix, mask,, GW, comment, etc...)
- Input subnet files without a CSV header are assumed to be : prefix;mask;GW;comment or prefix;mask;comment
- default CSV header is "prefix;mask;device;GW;comment"
- CSV header can be changed by using the configuration file (subnettools confdesc for more info)
- Input IPAM CSV MUST have a CSV header; there is a defaut header, but it is derived from my company's one
- So IPAM CSV header MUST be described in the configuration file

OUTPUT FMT
==========
- %I  : the prefix
- %N  : the network address of the prefix
- %B  : the 'broadcast' address of the prefix (last subnet IP, since IPv6 has no broadcast)
- %G  : the gateway (next-hop)
- %m  : the mask
- %M  : for IPv4, the mask in Dotted Decimal Notation; for IPv6 I DO REFUSE and print prefix length
- %D  : the device
- %C  : the comment

%I and %G MAY be followed by a IPv6 compression level (0, 1, or 2)

The character % MAY be followed by a field width (see printf man pages); this can help to align the results vertically, but please note in case width is smaller
than ouptut string, it WILL NOT be truncated

EXAMPLE 1 (stupid print):
-------------------------
[etienne@me]$ ./subnet-tools -fmt "HELLO my mask is %m my prefix is %I ma GW est %G, i say '%C'" route regtest/route_aggipv4

HELLO my mask is 23 my prefix is 10.1.0.0 ma GW est 192.168.1.1, i say 'AGGREGATE'

HELLO my mask is 24 my prefix is 10.1.2.0 ma GW est 192.168.1.1, i say 'test1'

HELLO my mask is 24 my prefix is 10.1.3.0 ma GW est 192.168.1.2, i say 'test1'

HELLO my mask is 22 my prefix is 10.1.4.0 ma GW est 192.168.1.2, i say 'AGGREGATE'

EXAMPLE 2 (using field width to align colons):
----------------------------------------------
Note : GitHub pretty print will not print that correcly, use a text editor

etienne@debian:~/st$ ./subnet-tools -fmt "%-16I;%-3m;%-10D;%-32G" route regtest/route_aggipv6-2 
2000:1::        ;32 ;eth0/1    ;fe80::254                      
 
2001:db4::      ;30 ;eth0/1    ;fe80::251                      
 
2001:db8::      ;32 ;eth0/2    ;fe80::251                      
 
2001:db9::      ;32 ;eth0/1    ;fe80::251                       

2001:dba::      ;31 ;eth0/1    ;fe80::251                  

EXAMPLE3 (converting subnets to address  ranges)
------------------------------------------------
[etienne@ARODEF subnet_tools]$ ./subnet-tools -fmt "%13I/%2m [%N-%B]" sort regtest/aggipv4 
      1.1.1.0/23 [1.1.0.0-1.1.1.255]

     10.0.0.0/24 [10.0.0.0-10.0.0.255]

     10.0.1.0/24 [10.0.1.0-10.0.1.255]

     10.0.2.0/23 [10.0.2.0-10.0.3.255]

     10.0.4.0/22 [10.0.4.0-10.0.7.255]


CODING
======
- in C because i like that, and i know only that
- coding style is close to linux kernel coding style
- the first version was a BASH script doing IPAM search and subnet file diff; it was sooooo slow
- will never be rewriten in C++, java, perl, etc...
- some code is clearly NIH; but i don't want to rely on linux or specific libraries so obvious functions a recoded
- all code is original except a few MACROs (from linux kernel); 
- original as in i did it myself, without copy&paste 
- is careful of verifiying every user input value so it doesnt crash

work TODO
=========
- make sure everything work
- fixing file diff (semantics is difficult, what do we want)
- adding more converters (and fixing IPv6 converters)
- fixing ECMP in converters (maybe)
- implement next_subnet, previous_subnet
- implement subnet_substract
- implement st_scanf & friends 
