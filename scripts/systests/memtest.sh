#!/bin/bash

logfile=memtest.log
progname=sliderule
testscript="scripts/selftests/test_runner.lua"
valgrind_suppressions="scripts/systests/memtest.supp"
valgrind_options="--leak-check=full --show-leak-kinds=all --track-origins=yes --track-fds=yes --error-exitcode=1 --suppressions=${valgrind_suppressions}"

printf "\nStarting memory test, output redirected to %s\n\n" $logfile

valgrind ${valgrind_options} ${progname} ${testscript} >${logfile} 2>&1

cat $logfile

#grep for error status, do not trust --error-exitcode=1 since failed lua test will cause 
#valgrind to return an error and not what we expect (0 since valgrind did not find any errors)
grep -w "ERROR SUMMARY: 0 errors" ${logfile}
ret=$?

if [ $ret == 0 ]; 
then
    printf "\nMemory test PASSED\n"
else
    printf "\nMemory test FAILED\n"
fi

exit $ret
