local runner = require("test_executive")
local console = require("console")

--console.monitor:config(core.LOG, core.DEBUG)
--sys.setlvl(core.LOG, core.DEBUG)

-- Table Unit Test --

runner.command("NEW UT_STRING ut_string")
runner.command("ut_string::REPLACEMENT")
runner.command("DELETE ut_string")

-- Report Results --

runner.report()

