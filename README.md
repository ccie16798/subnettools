subnettools
===========

subnet file tool !
IPv4/IPv6 subnet calculator, CSV route file manipulation and modification tool

FEATURES
========
Subnettools is a sotfware intending to help networ engineers manipulating route table and extract 
information from it; subnettools has :
- native IPv6 & IPv4 support
- IPv4 & IPv6 address information (known subnet membership, decoding of embedded IPs like Teredo)
- Powerfull pattern matching engine

Subnet arithmetic
-----------------
relation IP1 IP2    : prints a relationship between IP1 and IP2

split S, <l1,l2,..> : split subnet S l1 times, the result l2 times, and so on..

split2 S, <m1,m2,..>: split subnet S with mask m1, then m2, and so on...

removesub TYPE O1 S1: remove Subnet S from Object O1; if TYPE=file O1=ile, if TYPE=subnet 01=subnet

ipinfo IP|all|IPvX  : prints information about IP, or all known subnets (all, IPv4 or IPv6)

Route file simplification
-------------------------
sort FILE1          : sort CSV FILE1
sortby name file    : sort CSV file by (prefix|gw|mask), prefix is always a tie-breaker
sortby help	    : print available sort options
subnetagg FILE1     : sort and aggregate subnets in CSV FILE1; GW is not checked
routeagg  FILE1     : sort and aggregate subnets in CSV FILE1; GW is checked
simplify1 FILE1     : simplify CSV subnet file FILE1; duplicate or included networks are removed; GW is checked
simplify2 FILE1     : simplify CSV subnet file FILE1; prints redundant routes that can be removed

Route file comparison
---------------------
compare FILE1 FILE2 : compare FILE1 & FILE2, printing subnets in FILE1 INCLUDED in FILE2
missing FILE1 FILE2 : prints subnets from FILE1 that are not covered by FILE2; GW is not checked
ipam <IPAM> FILE1   : load IPAM, and print FILE1 subnets with comment extracted from IPAM
diff FILE1 FILE2    : diff FILE1 & FILE2 (EXPERIMENTAL)
common FILE1 FILE2  : merge CSV subnet files FILE1 & FILE2; prints common routes only; GW isn't checked
addfiles FILE1 FILE2: merge CSV subnet files FILE1 & FILE2; prints the sum of both files
grep FILE prefix    : grep FILE for prefix/mask
filter FILE EXPR    : grep netcsv   FILE using regexp EXPR
filter help         : prints help about bgp filters
bgpfilter FILE EXPR : grep bgp_file FILE using regexp EXPR
bgpfilter help      : prints help about bgp filters


subnettools FILE format is a CSV where each line represent a route ; a route is
-* a subnet
-* a subnet mask
-* a gateway
-* a device
-* a comment 
- delimitors, name of the fields describing (prefix, mask, gw, dev, comment) are fully configurable
- subnettools output format is configurable ; you can configure a FMT (%I %m %D %G %C)
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

Input CSV format
================
When working on CSV route files, please note the following
- Input subnet/routes files SHOULD have a CSV header describing its structure (prefix, mask,, GW, comment, etc...)
- Input subnet/routes files without a CSV header are assumed to be : prefix;mask;GW;comment or prefix;mask;comment
- default CSV header is "prefix;mask;device;GW;comment"
- CSV header can be changed by using the configuration file (subnettools confdesc for more info)
- Input IPAM CSV MUST have a CSV header; there is a defaut header, but it is derived from my company's one
- So IPAM CSV header MUST be described in the configuration file

- some commands can take <stdin> as input : sort, sortby, bgpsortby, print, ipam, filter, bgpfilter
OUTPUT FMT
==========
The output of subnettools command working on route files can be dynamically modified by supplying an output FMT
Some theory first, next some examples :)
- %I  : the prefix
- %N  : the network address of the prefix
- %B  : the 'broadcast' address of the prefix (last subnet IP, since IPv6 has no broadcast)
- %G  : the gateway (next-hop)
- %m  : the subnet mask (prefix length notation)
- %M  : for IPv4, the mask in Dotted Decimal Notation; for IPv6 I DO REFUSE and print prefix length
- %D  : the device
- %C  : the comment
- %U  : upper subnet of %I (lower subnet of 10.1.3.0/24 is 10.1.2.0/24)
- %L  : lower subnet 
- %P  : prefix/mask
%I, %N, %B and %G MAY be followed by a IPv6 address compression level (0, 1, 2, 3)

- compression level 0 : No compression at all
- compression level 1 : remove leading 0
- compression level 2 : remove consecutives 16bits blocks of zero
- compression level 3 : level 2 + print IPv4 Mapped & IPv4 compatible address in mixed IPv4 / IPv6 format

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

	etienne@debian:~/st$ ./subnet-tools -fmt "%-16I;%-3m;%-10D;%-32G" route regtest/route_aggipv6-2 
	2000:1::        ;32 ;eth0/1    ;fe80::254                      
	2001:db4::      ;30 ;eth0/1    ;fe80::251                      
	2001:db8::      ;32 ;eth0/2    ;fe80::251                      
	2001:db9::      ;32 ;eth0/1    ;fe80::251                       
	2001:dba::      ;31 ;eth0/1    ;fe80::251                  

EXAMPLE3 (converting subnets to address ranges)
-----------------------------------------------
[etienne@ARODEF subnet_tools]$ ./subnet-tools -fmt "%13I/%2m [%N-%B]" sort regtest/aggipv4 
   
     1.1.1.0/23  [1.1.0.0-1.1.1.255]
     10.0.0.0/24 [10.0.0.0-10.0.0.255]
     10.0.1.0/24 [10.0.1.0-10.0.1.255]
     10.0.2.0/23 [10.0.2.0-10.0.3.255]
     10.0.4.0/22 [10.0.4.0-10.0.7.255]

EXAMPLE 4 (playing with IPv6 address compression)
-------------------------------------------------

	etienne@debian:~/st$ ./subnet-tools echo "%I3" 2001::1
 	2001::1
	etienne@debian:~/st$ ./subnet-tools echo "%I1" 2001::1 
	2001:0:0:0:0:0:0:1
	etienne@debian:~/st$ ./subnet-tools echo "%I0" 2001::1 
	2001:0000:0000:0000:0000:0000:0000:0001
	etienne@debian:~/st$ ./subnet-tools echo "%I2" ::ffff:10.1.1.1
	::ffff:a01:101
	etienne@debian:~/st$ ./subnet-tools echo "%I3" ::ffff:10.1.1.1 
	::ffff:10.1.1.1

IP information
==============
subnettools ipinfo 'subnet' can give the following informations about 'subnet'
- network address, last subnet address
- known prefix membership (IPv6 global addresses, RFC 1918 private subnet, multicast, etc...)
Decoding of following addresses :
- Teredo
- IPv6 multicast (including embedded RP)
- 6to4
- ISATAP link-local address
- IPv4 glop
- rfc 6052
- EUI-64 Interface ID

Example :
---------

	[etienne@ARODEF subnet_tools]$ ./subnet-tools ipinfo 2001:0000:4136:e378:8000:63bf:3fff:fdd2 
	IP version : 6
	Network Address : 2001:0:4136:e378:8000:63bf:3fff:fdd2/128
	Address   Range : 2001:0:4136:e378:8000:63bf:3fff:fdd2 - 2001:0:4136:e378:8000:63bf:3fff:fdd2
	Previous subnet : 2001:0:4136:e378:8000:63bf:3fff:fdd1/128
	Next     subnet : 2001:0:4136:e378:8000:63bf:3fff:fdd3/128
	IPv6 rfc4380 Teredo
	Teredo server : 65.54.227.120
	Client IP     : 192.0.2.45
	UDP port      : 40000

	[etienne@ARODEF subnet_tools]$ ./subnet-tools ipinfo FF72:340:2001:DB8:BEEF:FEED::32
	IP version : 6
	Network Address : ff72:340:2001:db8:beef:feed:0:32/128
	Address   Range : ff72:340:2001:db8:beef:feed:0:32 - ff72:340:2001:db8:beef:feed:0:32
	Previous subnet : ff72:340:2001:db8:beef:feed:0:31/128
	Next     subnet : ff72:340:2001:db8:beef:feed:0:33/128
	IPv6 multicast address
	Scope : Link Local
	Flags : 7
	R=1, Embedded RP
	P=1, based on network prefix
	T=1, dynamically assigned prefix
	Embedded RP Address : 2001:db8:beef:feed::3
	32-bit group id 0x32 [50]

ROUTE FILTERING
===============

Subnettool is able to filter routes from a CSV files with complex expressions.
It can filter on :
- prefix
- mask
- gw
- device
- comment

For BGP, it can filter on :
- prefix
- mask
- gw
- LOCAL_PREF, MED, weight
- Valid, Best
- AS_PATH (=, <, > and # compare AS_PATH length; to compare actual AS_PATH, use '~')


operator are :
- '=' (EQUALS)
- '#' (DIFFERENT)
- '<' (numerically inferior)
- '>' (numerically superior)
- '{' (is included (for prefixes))
- '}' (includes (for prefixes))
- '~' (st_scanf regular expression)

the format ot the filter is :
- (A|B) : A or  B
- (A&B) : A and B
- !(A)  : not A

escape char for special chars is '\\'

some examples :
---------------
	Find any subnet included in 2001:db8::/48
	
	[etienne@ARODEF subnet_tools]$ ./subnet-tools   filter filter1 "prefix{2001:db8::0/48"
	2001:db8::;64;Ethernet1/0;2001::1;test1
	2001:db8::;128;Lo0;::;test1

	Find anything routed through Ethernet1/0
	etienne@ARODEF subnet_tools]$ ./subnet-tools   filter filter1 "device=Ethernet1/0"
	2001:db8::;64;Ethernet1/0;2001::1;test1
	2001:db8:1::;64;Ethernet1/0;2001::1;test1
	2001:db8:2::;64;Ethernet1/0;2001::2;test2
	2001:db8:8::;64;Ethernet1/0;2001::3;test3
	2001:db8:a::;64;Ethernet1/0;2001::3;test3
	2001:db8:e::;64;Ethernet1/0;2001::2;test2

	combine the two previous examples 
	OR
	[etienne@ARODEF subnet_tools]$ ./subnet-tools   filter filter1 "device=Ethernet1/0|prefix{2001:db8::0/48"
	2001:db8::;64;Ethernet1/0;2001::1;test1
	2001:db8::;128;Lo0;::;test1
	2001:db8:1::;64;Ethernet1/0;2001::1;test1
	2001:db8:2::;64;Ethernet1/0;2001::2;test2
	2001:db8:8::;64;Ethernet1/0;2001::3;test3
	2001:db8:a::;64;Ethernet1/0;2001::3;test3
	2001:db8:e::;64;Ethernet1/0;2001::2;test2

	AND
	[etienne@ARODEF subnet_tools]$ ./subnet-tools   filter filter1 "device=Ethernet1/0&prefix{2001:db8::0/48"
	2001:db8::;64;Ethernet1/0;2001::1;test1
	
	
	

CODING
======
- subnettools is in C because i like that, and i know only that
- coding style is close to linux kernel coding style
- the first version was a BASH script doing IPAM search and subnet file diff; it was sooooo slow
- will never be rewritten in C++, java, perl, etc...at least by me
- some code is clearly NIH; but i don't want to rely on linux or specific libraries so obvious functions a recoded
- all code is original except a few MACROs (from linux kernel) and some doc taken from scanf/printf pages
- original as in i wrote it myself, without copy&paste
- is careful of verifiying every user input value so it doesnt crash

work TODO
=========
- fixing file diff (semantics is difficult, what do we want)
- adding more converters (and fixing IPv6 converters)
- bitmap printing
- implement progress bar (what is your 1M line aggregation doing??)
- code precision in st_printf.c
