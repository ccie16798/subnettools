#! /bin/bash


PROG='../subnet-tools '
n_ok=0
n_ko=0

reg_test() {
	local output_file
	local ref_file
	local conf_name

	if [ $1 = "-c" ]; then
		conf_name=$2
		shift
		shift
	else
		conf_name=st.conf
	fi
	output_file=`echo $@| sed s/\ /_/g | tr / -`
	echo -n "reg test [$@] : "
	if [ ! -f ref/$output_file ]; then
		echo "No ref file found for this test, creating it '$ref_file'"
		$PROG -c $conf_name $1 $2 $3 $4 > ref/$output_file	
		return
	fi
	$PROG -c $conf_name $1 $2 $3 $4 > res/$output_file
	diff res/$output_file ref/$output_file > /dev/null
	if [ $? -eq 0 ]; then
		echo -e "\033[32mOK\033[0m"
		n_ok=$((n_ok + 1))
	else
		echo -e "\033[31mKO\033[0m"
		n_ko=$((n_ko + 1))
	fi
}

reg_test_scanf() {
	local output_file
	local n

	n=30
	$PROG scanf "1.1.1.1 zob    1.1.1.2    name 25" " *%I (%S )?.*%I *(name) %d" > res/scanf1 
	$PROG scanf "1.1.1.1   1.1.1.2    name 25" " *%I (%S )?.*%I *(name) %d" > res/scanf2 
	$PROG scanf "1.1.1.1  1.1.1.2 2.2.2.2 toto   r" " *%I .*%S" > res/scanf3
	$PROG scanf "1.1.1.1  1.1.1.2 2.2.2.2 toto   r" " *%I .*%[a-z]" > res/scanf4
	# test last string matching .*$%s
	$PROG scanf "i L2     10.73.0.6/32 [115/200] via 10.73.10.106, 5d08h, Vlan860" ".*%P.*(via )%I.*$%s" > res/scanf5
	# test conversion specifier inside a ( )? construct and %S conversion specifier
	$PROG scanf "ip route vrf TRUC 10.1.1.0 255.255.248.0 192.168.1.3 tag 8 name HELLO" "ip route.*%I %M (%S )?.*%I.*(name).*%s" > res/scanf6
	$PROG scanf "ip route vrf TRUC 10.1.1.0 255.255.248.0 Vlan38 192.168.1.3 tag 8 name HELLO" "ip route.*%I %M (%S )?.*%I.*(name).*%s" > res/scanf7
	$PROG scanf "ip route 100.1.1.0 255.248.0.0 192.168.1.3 tag 8 name HELLO" "ip route.*%I %M (%S )?.*%I.*(name).*%s" > res/scanf8
	$PROG scanf "ip route 100.1.1.0 255.248.0.0 Vlan38 192.168.1.3 tag 8 name HELLO" "ip route.*%I %M (%S )?.*%I.*(name).*%s" > res/scanf9
	# test char range and end on char range
	$PROG scanf "1234567891242434244244" ".*%[2-4]" >  res/scanf10
	$PROG scanf "1234567891242434244244" ".*$%[2-4]" >  res/scanf11
	# end on IPv6
	$PROG scanf "ddfsdfsdf   2001:2:3::1" ".*$%I" > res/scanf12
	$PROG scanf "ab]ca 1.1.1.1" "%[]abc].*%I" > res/scanf13
	# scanf with a ] in a char range
	$PROG scanf "ab]ca1.1.1.1" "[]abc]*%I" > res/scanf14
	# exercise '%Q'
	$PROG scanf " 1.1/16 1.1.1.1 toto" " *%Q.*$%S" > res/scanf15
	#exercise End on char range (the range below matches 'f')
	$PROG scanf "abcdef1.1.1.1"   ".*[f-g]%I"  > res/scanf16
	$PROG scanf "abcdef1.1.1.1"   ".*[^a-e]%I" > res/scanf17
	# test short and signed, unsigned
	$PROG scanf  "  65000  3000000000  3000000000" ".*%hd.*%d.*%u" > res/scanf18
	# test max field length
	$PROG scanf  "1234567890  abcdabcdabcd 1234567890"  "%9s[90 ]*%5[abcd](abcdabcd).*%9S" > res/scanf19
	# test HEX scanning
	$PROG scanf "0xfffeabc 200 0xfff1fffe" "%x *%hx .*$%lx" > res/scanf20
	# test OR  result is '12 1.1.1.1 1234'
	$PROG scanf "12 1.1.1.1 aa 1234" "(bozz|ss|%d %I) aa %d " > res/scanf21
	# %d %S doesnt matchn %d %I does
	$PROG scanf "12 1.1.1.1 aa 1234" "(bozz|ss|%d %S|%d %I) aa %d " > res/scanf22
	# {m,n} multiplier
	$PROG scan "1234567890a" "[0-9]{1}%c" > res/scanf23
	$PROG scan "1234567890a" "[0-9]{2}%c" > res/scanf24
	$PROG scan "1234567890a" ".{2}%c" > res/scanf25
	$PROG scan "1234567890a" ".{3}%c" > res/scanf26
	$PROG scan "1234567890a" ".{3,4}%c" > res/scanf27
	$PROG scan "1234567890a" ".{,4}%c" > res/scanf28
	$PROG scan "1234567890a" ".{4,}%c" > res/scanf29
	#complex end of char range returns ' local hh'
	$PROG scan "    *via 10.24.133.5, Vlan600, [0/0], 1y7w, local hh" ".*$%[^,]" > res/scanf30


	for i in `seq 1 $n`; do
		output_file=scanf$i
		if [ ! -f ref/$output_file ]; then
			echo "No ref file found for this test, creating it 'ref/$output_file'"
			cp res/$output_file ref/$output_file
		else
			echo -n "reg test [scanf #$i] :"
			diff res/$output_file ref/$output_file > /dev/null
			if [ $? -eq 0 ]; then
				echo -e "\033[32mOK\033[0m"
				n_ok=$((n_ok + 1))
			else
				n_ko=$((n_ko + 1))
				echo -e "\033[31mKO\033[0m"
			fi
		fi
	done
	
}

reg_test_logic() {
	local output_file
	local n

	$PROG exprtest "1=1&3>2" > res/logic1
	$PROG exprtest "1=0&3>2" > res/logic2
	$PROG exprtest "1=0&((3>2)&(4=4))" > res/logic3
	$PROG exprtest "1=1&(21=20|3<4&(4>5))" > res/logic4
	$PROG exprtest "1=0|(21=20|3<4&(4>5))" > res/logic5
	$PROG exprtest "1=1&(21=20|3<4&(4>3))" > res/logic6
	n=6
	for i in `seq 1 $n`; do
		output_file=logic$i
		if [ ! -f ref/$output_file ]; then
			echo "No ref file found for this test, creating it 'ref/$output_file'"
			cp res/$output_file ref/$output_file
		else
			echo -n "reg test [logic #$i] :"
			diff res/$output_file ref/$output_file > /dev/null
			if [ $? -eq 0 ]; then
				echo -e "\033[32mOK\033[0m"
				n_ok=$((n_ok + 1))
			else
				n_ko=$((n_ko + 1))
				echo -e "\033[31mKO\033[0m"
			fi
		fi
	done
}

result() {
	echo "Summary : "
	echo -e "\033[32m$n_ok OK\033[0m"
	echo -e "\033[31m$n_ko KO\033[0m"
}


#test for IPv4/IPv6 handling
reg_test print invalid_ips_masks.txt
#basic print to test fmt
reg_test -c st-fmt.conf print route_aggipv6-2
reg_test -c st-fmt.conf print route_aggipv4
# a CSV with strange fields names :)
reg_test -c st-bizarr.conf sort bizar.csv
# a CSV with strange fields names, output more strange
result() {
	echo "Summary : "
	echo -e "\033[32m$n_ok OK\033[0m"
	echo -e "\033[31m$n_ko KO\033[0m"
}


#test for IPv4/IPv6 handling
reg_test print invalid_ips_masks.txt
#basic print to test fmt
reg_test -c st-fmt.conf print route_aggipv6-2
reg_test -c st-fmt.conf print route_aggipv4
# a CSV with strange fields names :)
reg_test -c st-bizarr.conf sort bizar.csv 
# a CSV with strange fields names, output more strange 
reg_test -c st-bizarr2.conf sort bizar2.csv
reg_test missing  BURP2 BURP
reg_test common  BURP2 BURP
reg_test subnetagg aggipv4
# this one should test enough IPv6 functionnality
reg_test subnetagg aggipv6
reg_test subnetagg bigcsv
reg_test routeagg route_aggipv6
reg_test routeagg route_aggipv6-2
reg_test routeagg route_aggipv4
reg_test simplify1 BURP
reg_test simplify2 BURP
reg_test simplify1 simple
reg_test simplify2 simple
reg_test sort aggipv4
reg_test sortby prefix	sortme
reg_test sortby mask	sortme
reg_test sortby gw	sortme
reg_test simplify1 simplify1
reg_test simplify2 simplify1
reg_test sort aggipv6
reg_test sort sort1
reg_test sort sort1-ipv6
# removal
reg_test remove subnet 10.1.1.2/16  10.1.2.0/24
reg_test remove subnet 10.1.0.0/16  10.1.0.0/28
reg_test remove subnet 10.1.0.0/16  10.1.255.240/28
reg_test remove subnet 2001:db8::/32 2001:db8::/64
reg_test remove subnet 2001:db8::/32 2001:db8:a::/64
reg_test remove subnet 2001:db8::/32 2001:db8:ffff:ffff::/64
reg_test remove file route_aggipv6-2 2001:dbb::/64
reg_test remove file route_aggipv4 10.1.4.0/32

reg_test split 2001:db8:1::/48 16,16,16
reg_test split 2001:db8:1::/48 16
reg_test split 10.2.0.0/16 256,4

# scanf
reg_test_scanf
#logic test
reg_test_logic

# converter
reg_test convert CiscoRouterconf	ciscorouteconf_v4
reg_test convert CiscoRouterconf	ciscorouteconf_v6
reg_test convert ciscofw		iproute_ciscofw
reg_test convert ciscofw		iproute_ASA
reg_test convert ciscofwconf		ciscofwconf_route_v4.txt 
reg_test convert ciscofwconf		ciscofwconf_route_v6.txt 
reg_test convert CiscoRouter		iproute_cisco
reg_test convert CiscoRouter		iproute_cisco_ECMP 
reg_test convert CiscoRouter		ipv6route2
reg_test -rt convert CiscoRouter	iproute_cisco
reg_test -rt convert CiscoRouter	iproute_cisco_ECMP 
reg_test -rt convert CiscoRouter	ipv6route2
reg_test -rt convert ciscofw		iproute_ASA
reg_test -rt convert cisconexus		iproute_nexus2
reg_test -rt convert gaia 		iproute_gaia_R77
reg_test -ecmp convert cisconexus	iproute_nexus2
reg_test -ecmp convert ciscorouter	iproute_cisco_ECMP
reg_test -ecmp convert gaia 		iproute_gaia_R77
reg_test convert IPSO iproute_nokia
reg_test convert gaia iproute_gaia_R77
reg_test convert CiscoNexus iproute_nexus
reg_test convert CiscoNexus iproute_cisconexus_ECMP
reg_test convert palo iproute_palo
reg_test convert ciscobgp ciscobgp
reg_test bgpcmp  bgp1 bgp2
reg_test bgpsortby med bgp1
reg_test bgpsortby prefix bgp1
reg_test bgpsortby localpref bgp1
reg_test bgpsortby gw bgp1

reg_test grep mergeipv6 2001:db8::
#ipinfo

reg_test ipinfo 2001:0000:4136:e378:8000:63bf:3fff:fdd2
reg_test ipinfo FF72:340:2001:DB8:BEEF:FEED::32
reg_test ipinfo 64:ff9b::A00A:0A01
reg_test ipinfo 2002:C0A8:1B0B::2001:10
reg_test ipinfo 233.10.56.1
reg_test ipinfo 10.0.2.1/16
reg_test ipinfo 127.0.2.1/24
reg_test ipinfo fe80::215:afff:fedb:3b9e/64

result
