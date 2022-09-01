local runner = require("test_executive")
local td = runner.rootdir(arg[0]) -- root directory
local console = require("console")

-- Run ICESat2 Unit Tests --

runner.script(td .. "plugin_unittest.lua")
runner.script(td .. "atl06_elements.lua")
runner.script(td .. "atl03_indexer.lua")

-- Report Results --

runner.report()

-- Cleanup and Exit --

sys.quit()
