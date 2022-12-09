#!/bin/bash

logfile=memtest.log
progname=sliderule
kinds="definite,indirect"
testscript="scripts/selftests/test_runner.lua"
valgrind_suppressions="scripts/systests/memtest.supp"
valgrind_options="--leak-check=full --show-leak-kinds=${kinds} --track-origins=yes --track-fds=yes --log-file=${logfile} --suppressions=${valgrind_suppressions}"

printf "\nStarting memory test, valgrind output redirected to %s\n\n" $logfile

valgrind ${valgrind_options} ${progname} ${testscript}

#grep for error status, do not trust --error-exitcode=1 since failed lua test will cause
#valgrind to return an error and not what we expect (0 since valgrind did not find any errors)
grep -q -w "ERROR SUMMARY: 0 errors" ${logfile}
ret=$?


printf "\n\n********* valgrind output from %s *********\n\n" $logfile
cat $logfile

if [ $ret == 0 ];
then
    printf "\nMemory test PASSED\n"
else
    printf "\nMemory test FAILED\n"
fi

exit $ret
