local runner = require("test_executive")
local aws_utils = require("aws_utils")
local _,dir = runner.srcscript()

-- Configure Optional Tags
local tags = {}
for i=1,#arg do table.insert(tags, arg[i]) end
if #tags > 0 then
    runner.setscope(tags)
end

-- Configure Running In Cloud --
aws_utils.config_aws()

-- Execute Tests --
runner.script(dir .. "atl24_writer.lua")
runner.script(dir .. "atl24_uncertainty.lua")
runner.script(dir .. "atl24_granule.lua")

-- Report Results --
local errors = runner.report()

-- Cleanup and Exit --
sys.quit( errors )
