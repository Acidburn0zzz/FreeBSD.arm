#
# Regression tests for date(1)
#
# Submitted by Edwin Groothuis <edwin@FreeBSD.org>
#
# $FreeBSD: head/bin/date/tests/format_string_test.sh 269903 2014-08-13 04:43:29Z ngie $
#

#
# These two date/times have been chosen carefully -- they
# create both the single digit and double/multidigit version of
# the values.
#
# To create a new one, make sure you are using the UTC timezone!
#

TEST1=3222243		# 1970-02-07 07:04:03
TEST2=1005600000	# 2001-11-12 21:11:12

check()
{
	local format_string exp_output_1 exp_output_2

	format_string=${1}
	exp_output_1=${2}
	exp_output_2=${3}

	atf_check -o "inline:${exp_output_1}\n" \
	    date -r ${TEST1} +%${format_string}
	atf_check -o "inline:${exp_output_2}\n" \
	    date -r ${TEST2} +%${format_string}
}

format_string_test()
{
	local desc exp_output_1 exp_output_2 flag

	desc=${1}
	flag=${2}
	exp_output_1=${3}
	exp_output_2=${4}

	atf_test_case ${desc}_test
	eval "
${desc}_test_body() {
	check ${flag} '${exp_output_1}' '${exp_output_2}';
}"
	atf_add_test_case ${desc}_test
}

atf_init_test_cases()
{
	format_string_test A A Saturday Monday
	format_string_test a a Sat Mon
	format_string_test B B February November
	format_string_test b b Feb Nov
	format_string_test C C 19 20
	format_string_test c c "Sat Feb  7 07:04:03 1970" "Mon Nov 12 21:20:00 2001"
	format_string_test D D 02/07/70 11/12/01
	format_string_test d d 07 12
	format_string_test e e " 7" 12
	format_string_test F F "1970-02-07" "2001-11-12"
	format_string_test G G 1970 2001
	format_string_test g g 70 01
	format_string_test H H 07 21
	format_string_test h h Feb Nov
	format_string_test I I 07 09
	format_string_test j j 038 316
	format_string_test k k " 7" 21
	format_string_test l l " 7" " 9"
	format_string_test M M 04 20
	format_string_test m m 02 11
	format_string_test p p AM PM
	format_string_test R R 07:04 21:20
	format_string_test r r "07:04:03 AM" "09:20:00 PM"
	format_string_test S S 03 00
	format_string_test s s ${TEST1} ${TEST2}
	format_string_test U U 05 45
	format_string_test u u 6 1
	format_string_test V V 06 46
	format_string_test v v " 7-Feb-1970" "12-Nov-2001"
	format_string_test W W 05 46
	format_string_test w w 6 1
	format_string_test X X "07:04:03" "21:20:00"
	format_string_test x x "02/07/70" "11/12/01"
	format_string_test Y Y 1970 2001
	format_string_test y y 70 01
	format_string_test Z Z UTC UTC
	format_string_test z z +0000 +0000
	format_string_test percent % % %
	format_string_test plus + "Sat Feb  7 07:04:03 UTC 1970" "Mon Nov 12 21:20:00 UTC 2001"
}
