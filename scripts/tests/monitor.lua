local runner = require("test_executive")
local console = require("console")

-- Unit Test --

sys.log(core.CRITICAL, "Hello World")

local monitor = core.getbyname("console.monitor")

sys.wait(1)
print("Starting CAT")
monitor:cat()

-- Clean Up --

-- Report Results --

runner.report()

