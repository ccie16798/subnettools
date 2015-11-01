#! /bin/sh

TEST_READ=../test-read

test_diff() {
	diff $1 $2 > /dev/null
	if [ $? -eq 0 ]; then
		echo -e "\033[32mOK\033[0m"
	else
		echo -e "\033[31mKO\033[0m"
	fi
}


echo -n BIG_CSV : 
$TEST_READ bigcsv > res/bigcsv
test_diff bigcsv res/bigcsv
echo -n "Trunc zob to 64 : "
$TEST_READ zob 64 > res/zob_64
test_diff ref/zob_64 res/zob_64
echo -n "Trunc zob to 70 : "
$TEST_READ zob 70 > res/zob_70
test_diff ref/zob_70 res/zob_70
echo -n "Trunc zob to 128 : "
$TEST_READ zob 128 > res/zob_128
test_diff ref/zob_128 res/zob_128
echo -n "pathetic file : "
$TEST_READ pathetic_file > res/pathetic_file
test_diff ref/pathetic_file res/pathetic_file
echo -n "pathetic file 79 : "
$TEST_READ pathetic_file 79 > res/pathetic_file_79
test_diff ref/pathetic_file_79 res/pathetic_file_79
echo -n "pathetic file 77 : "
$TEST_READ pathetic_file 77 > res/pathetic_file_77
test_diff ref/pathetic_file_77 res/pathetic_file_77
