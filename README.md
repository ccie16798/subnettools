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
- %I : the prefix
- %[0-2]I : the prefix, but with 0-2 compression for IPv6
- %G : the gateway (next-hop)
- %[0-2]G : the gateway (next-hop) 
- %m : the mask
- %D : the device
- %C : the comment

EXAMPLE:
--------
[etienne@me]$ ./subnet-tools -fmt "HELLO my mask is %m my prefix is %I ma GW est %G, i say '%C'" route regtest/route_aggipv4
HELLO my mask is 23 my prefix is 10.1.0.0 ma GW est 192.168.1.1, i say 'AGGREGATE'
HELLO my mask is 24 my prefix is 10.1.2.0 ma GW est 192.168.1.1, i say 'test1'
HELLO my mask is 24 my prefix is 10.1.3.0 ma GW est 192.168.1.2, i say 'test1'
HELLO my mask is 22 my prefix is 10.1.4.0 ma GW est 192.168.1.2, i say 'AGGREGATE'


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


