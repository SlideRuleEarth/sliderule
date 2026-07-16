local runner = require("test_executive")

-- Requirements --

if (not core.UNITTEST) or (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

local atl06_dispatch = icesat2.ut_atl06()

-- Self Test --

runner.unittest("ATL06 Dispatch LSF Unit Test", function()
    runner.assert(atl06_dispatch:lsftest(), "Failed lsftest")
end)

runner.unittest("ATL06 Dispatch Sort Unit Test", function()
    runner.assert(atl06_dispatch:sorttest(), "Failed sorttest")
end)

-- Report Results --

runner.report()
