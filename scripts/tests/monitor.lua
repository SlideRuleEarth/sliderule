local runner = require("test_executive")
console = require("console")

-- Unit Test --

sys.log(core.CRITICAL, "Hello World")

local monitor = core.getbyname("console.monitor")

sys.wait(1)

hello_found = false
p,s = monitor:cat(1)
for _,msg in ipairs(p) do
    if msg:find("Hello World") then
        hello_found = true
    end
end

runner.check(hello_found, "Failed to find \"Hello World\" message")

-- Clean Up --

-- Report Results --

runner.report()

