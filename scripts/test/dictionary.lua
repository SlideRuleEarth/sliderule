local runner = require("test_executive")
local rd = runner.rootdir(arg[0])

-- Dictionary Unit Test --

runner.command("NEW UT_DICTIONARY ut_dictionary")
runner.command(string.format('ut_dictionary::ADD_WORD_SET small %s../../scripts/test/alphabet_words.txt', rd))
runner.command(string.format('ut_dictionary::ADD_WORD_SET large %s../../scripts/test/english_words.txt', rd))
runner.command("ut_dictionary::FUNCTIONAL_TEST small")
runner.command("ut_dictionary::FUNCTIONAL_TEST large")
runner.command("ut_dictionary::ITERATOR_TEST small")
runner.command("DELETE ut_dictionary")

-- Report Results --

runner.report()

