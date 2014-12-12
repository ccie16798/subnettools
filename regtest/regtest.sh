#! /bin/sh


PROG=../subnet-tools

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
	output_file=`echo $@| sed s/\ /_/g`
	ref_file=`echo $@| sed s/\ /_/g`
	echo -n "reg test [$@] : "
	if [ ! -f ref/$ref_file ]; then
		echo "No ref file found for this test, creating it '$ref_file'"
		$PROG -c $conf_name $1 $2 $3  > ref/$ref_file	
		return
	fi
	$PROG -c $conf_name $1 $2 $3  > res/$output_file
	diff res/$output_file ref/$ref_file > /dev/null
	if [ $? -eq 0 ]; then
		echo "\033[32mOK\033[0m"
	else
		echo "\033[31mKO\033[0m"
	fi
}
# a CSV with strange fields names :)
reg_test -c st-bizarr.conf sort bizar.csv 
reg_test missing  BURP2 BURP
reg_test common  BURP2 BURP
reg_test aggregate aggipv4
# this one should test enough IPv6 functionnality
reg_test aggregate aggipv6
reg_test aggregate bigcsv
reg_test simplify1 BURP
reg_test simplify2 BURP
reg_test simplify1 simple
reg_test simplify2 simple
reg_test sort aggipv4
reg_test sort aggipv6
reg_test read CiscoRouter iproute_cisco
reg_test read IPSO iproute_nokia
reg_test read CiscoNexus iproute_nexus
reg_test read CiscoRouter ipv6route2
reg_test grep mergeipv6 2001:db8::
