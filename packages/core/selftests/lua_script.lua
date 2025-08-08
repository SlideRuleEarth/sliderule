local runner = require("test_executive")
local json = require("json")

-- Self Test --

local script = core.script("api/health", "")

local total_time_allowed = 5
while script:active() and total_time_allowed > 0 do
    sys.wait(1)
    total_time_allowed = total_time_allowed - 1
end

local result = json.decode(script:result())

runner.assert(result.healthy == true, "Failed to retrieve results of script")

-- Clean Up --

-- Report Results --

runner.report()
