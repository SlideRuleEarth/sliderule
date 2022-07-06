local runner = require("test_executive")
local console = require("console")
local td = runner.rootdir(arg[0]) .. "../selftests"

-- Table Unit Test --

runner.command("NEW UT_LIST ut_list")
runner.command("ut_list::ADD_REMOVE")
runner.command("ut_list::DUPLICATES")
runner.command("ut_list::SORT")
runner.command("DELETE ut_list")

-- Report Results --

runner.report()

