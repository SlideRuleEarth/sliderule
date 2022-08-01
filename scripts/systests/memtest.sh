#!/bin/bash

logfile=memtest.log
progname=sliderule
testscript="scripts/selftests/test_runner.lua"
valgrind_suppressions="scripts/systests/memtest.supp"
valgrind_options="--leak-check=full --show-leak-kinds=all --track-origins=yes --track-fds=yes --error-exitcode=1 --suppressions=${valgrind_suppressions}"

printf "\nStarting memory test...\n\n"

#valgrind ${valgrind_options} ${progname} ${testscript} >${logfile} 2>&1
valgrind ${valgrind_options} ${progname} ${testscript} 
ret=$?

if [ $ret == 0 ]; 
then
    printf "\nMemory test PASSED\n"
else
    printf "\nMemory test FAILED\n"
fi

# return valgrind exit status
# --error-exitcode=1 will cause valgrind to return 1 on any errors or 0 when none
exit $ret
