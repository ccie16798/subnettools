subnettools
===========

subnet file tool !

a tools designed to help network engineer manipulate large route/subnet file
- compare files (common routes, missing routes)
- get a subnet file declaration from an IPAM file
- grep files for subnets
- simplifying route files (removing duplicates, sorting)
- aggregating subnet as much as possible
-converting 'sh ip route' files to a CSV

-subnettools main format is CSV; delimitors and field are fully configurable
-subnettools has a default config file (st.conf)
-subnettools has a debug system (subnettools -D help), mostly helped me to create it
-subnettools a small regression testing suite


LIMITS :
- file size : more than enough (i aggretated a 8millions line CSv in 2-3minutes)
- line size : 1024 (subnettools truncates too long lines)
it should work OK today for most functions

work TODO
-make sure everything work
-fixing file diff 
-adding more converters


