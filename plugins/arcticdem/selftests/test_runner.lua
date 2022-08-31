local runner = require("test_executive")
local rd = runner.rootdir(arg[0]) -- root directory
local td = rd .. "../selftests/" -- test directory
local console = require("console")

-- Run ArcticDEM Unit Tests --

runner.script(td .. "arcticdem_reader.lua")

-- Report Results --

runner.report()

-- Cleanup and Exit --

sys.quit()
