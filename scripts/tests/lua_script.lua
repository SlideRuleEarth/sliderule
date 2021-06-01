local runner = require("test_executive")
local console = require("console")
local json = require("json")

-- Unit Test --

script = core.script("health", "")

local total_time_allowed = 5
while script:active() and total_time_allowed > 0 do
    sys.wait(1)
    total_time_allowed = total_time_allowed - 1
end

result = json.decode(script:result())

runner.check(result.healthy == true, "Failed to retrieve results of script")

-- Clean Up --

-- Report Results --

runner.report()
