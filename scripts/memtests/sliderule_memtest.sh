#!/bin/bash

logfile=sliderule_valgrind.log
progname="build/targets/server-linux/sliderule"
testscript="scripts/selftests/test_runner.lua"
valgrind_suppressions="scripts/memtests/sliderule.supp"
valgrind_options="--leak-check=full --show-leak-kinds=all --track-origins=yes --track-fds=yes --error-exitcode=1 --suppressions=${valgrind_suppressions}"

printf "\nStarting memory test...\n"

valgrind ${valgrind_options} ./${progname} ${testscript} >${logfile} 2>&1
ret=$?

if [ $ret == 0 ]; 
then
    printf "Memory test PASSED\n"
else
    printf "Memory test FAILED, see $logfile for details\n"
fi

# return valgrind exit status
# --error-exitcode=1 will cause valgrind to return 1 on any errors or 0 when none
exit $ret
