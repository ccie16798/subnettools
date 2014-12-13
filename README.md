subnettools
===========

subnet file tool !

a tool designed to help network engineer manipulate large route/subnet CSV files
- compare files (common routes, missing routes)
- get a subnet file declaration from an IPAM file (IPAM format fully dynamic)
- grep files for subnets
- simplifying route files (removing duplicates, sorting)
- aggregating subnets as much as possible
- converting 'sh ip route' files to a CSV

- subnettools main format is CSV; delimitors and field are fully configurable
- subnettools supportss IPv4 and IPv6
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

work TODO
=========
-make sure everything work
-fixing file diff 
-adding more converters (and fixing IPv6 converters)


