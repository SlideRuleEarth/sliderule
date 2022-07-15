local runner = require("test_executive")
local console = require("console")
local json = require("json")

-- Unit Test --

local script = core.script("api/health", "") --:name("myscript")

local total_time_allowed = 5
while script:active() and total_time_allowed > 0 do
    sys.wait(1)
    total_time_allowed = total_time_allowed - 1
end

local result = json.decode(script:result())

print("My Result: ", result.healthy)

runner.check(result.healthy == true, "Failed to retrieve results of script")

-- Clean Up --

-- Report Results --

runner.report()
