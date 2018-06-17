#! /bin/bash

PROG='../subnet-tools '
#PROG='../subnet-tools -D memory:4 '
n_ok=0
n_ko=0

result() {
	echo "Summary : "
	echo -e "\033[32m$n_ok OK\033[0m"
	echo -e "\033[31m$n_ko KO\033[0m"
}

reg_test_scanf() {
	local output_file
	local n

	n=55
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
	#end on char range
	$PROG scan "a 1.1.11.1 abc" ".*%I.*[abc]%c" > res/scanf31
	$PROG scan "64.242.88.10 - - [07/Mar/2004:17:50:44 -0800] \"GET /twiki/bin/view/TWiki/SvenDowideit HTTP/1.1\" 200 3616" "%I.*\[%[^]].*\"%s %s" > res/scanf32
	$PROG scanf "3.3.3.3   2.2.2.2 1.1.1.1" ".*$%I" > res/scanf33
	$PROG scanf "2.2.2.2 1.1.1.1 a" "(%I )*%c" > res/scanf34
	# min matching tests
	$PROG scanf "123.1.1.200 1.1.1.1" ".{2,}%I" > res/scanf35
	$PROG scanf "123.1.1.200 1.1.1.1" ".{4,}%I" > res/scanf36
	# test expansion of escaped char
	$PROG scanf "[[[[[[[aaa" "\[{2,}%s" > res/scanf37
	# end on long (with a stupid field length)
	$PROG scanf "azazz112222" ".*%10ld" > res/scanf38
	# special chars
	$PROG scanf "ab[e" "ab\[*%c" > res/scanf39
	$PROG scanf "ab[e" "ab\[%c" > res/scanf40
	$PROG scanf "abcad" "abcad(a)+" > res/scanf41
	$PROG scanf "abcad" "abcad(a)*" > res/scanf42
	#test INT types
	$PROG scanf "ff ab22 0xff42ff42"  "%x %hx %lx" > res/scanf43
	$PROG scanf "1234 -34 3456 54612122122"  "%d %hd %hd %ld" > res/scanf44
	#test max field length
	$PROG scanf "abcdef ghijk abcde" ".*%5W.* %3s[a-z]* %5[a-e]" > res/scanf45
	$PROG scanf "255.255.0.0 255.128.0.0 9" "%M %M %M" > res/scanf46
	#end on expression
	$PROG scanf "1123 3.3.3.3 456" ".*( %I %d)" > res/scanf47
	$PROG scanf "1123 3.3.3.3 456  1.1.1.1 45" '.*$( %I %d)' > res/scanf48
	$PROG scanf "1123 3.3.3.3 456  111.1.1.1 45" '.*$( %I %d)' > res/scanf49
	$PROG scanf "wyabced-efghthty" "%[wya-f-]" > res/scanf50
	$PROG scanf "wy[abced-efghthty" "(%[[wya-f-])" > res/scanf51
	$PROG scanf "wy[abced-efghthty" "(%[^a-f-])" > res/scanf52
	$PROG scanf "ab 223" "a\b %d" > res/scanf53
	#this should match
	$PROG scanf "1111" "1{1,4}.*" > res/scanf54
	# ( in an expression is not interpreted
	$PROG scanf  "(2.2.2.2) (1.1.1.1) a" "((%I) )*%c" > res/scanf55

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

reg_test_scanf
result
