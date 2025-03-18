local runner = require("test_executive")

-- Self Test --

sys.log(core.CRITICAL, "Hello World")

local monitor = core.getbyname("console.monitor")

sys.wait(1)

local hello_found = false
local p,s = monitor:cat(1)
for _,msg in ipairs(p) do
    if msg:find("Hello World") then
        hello_found = true
    end
end

runner.assert(hello_found, "Failed to find \"Hello World\" message")

-- Clean Up --

-- Report Results --

runner.report()
