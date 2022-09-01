local runner = require("test_executive")
local console = require("console")

-- Table Unit Test --

runner.command("NEW UT_TABLE ut_table")
runner.command("ut_table::ADD_REMOVE")
runner.command("ut_table::CHAINING")
runner.command("ut_table::REMOVING")
runner.command("ut_table::DUPLICATES")
runner.command("ut_table::FULL_TABLE")
runner.command("ut_table::COLLISIONS")
runner.command("ut_table::STRESS")
runner.command("DELETE ut_table")

-- Report Results --

runner.report()

