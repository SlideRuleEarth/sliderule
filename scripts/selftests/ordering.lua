local runner = require("test_executive")
local console = require("console")

--console.monitor:config(core.LOG, core.DEBUG)
--sys.setlvl(core.LOG, core.DEBUG)

-- Table Unit Test --

runner.command("NEW UT_ORDERING ut_ordering")
runner.command("ut_ordering::ADD_REMOVE")
runner.command("ut_ordering::DUPLICATES")
runner.command("ut_ordering::SORT")
runner.command("ut_ordering::ITERATE")
runner.command("DELETE ut_ordering")

-- Report Results --

runner.report()

