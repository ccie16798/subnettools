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

- subnettools main format is CSV; delimitors and field are fully configurable
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
- subnet files SHOULD have a CSV header describing its structure (prefix, mask,, GW, comment, etc...)
- subnet files without a CSV header are assumed to be : prefix;mask;GW;comment
- default CSV header is "prefix;mask;device;GW;comment"
- CSV header can be change byt using the configuration file (subnettools confdesc for more info)
- IPAM CSV MUST have a CSV header; there is a defaut header, but it is derived from my company's one
- thus IPAM CSV header MUST be described in the configuration file

CODING
======
- in C because i like that, and i know only that
- the first version was a BASH script doing IPAM search and subnet file diff; it was sooooo slow
- will never be rewriten in C++, java, perl, etc...
- some code is clearly NIH; but i don't want to rely on linux or specific libraries so obvious function a re recoded
- is careful of verifiying every user input value so it doesnt crash

work TODO
=========
- make sure everything work
- fixing file diff (semantics is difficult, what do we want)
- adding more converters (and fixing IPv6 converters)
- complete dynamic output format


