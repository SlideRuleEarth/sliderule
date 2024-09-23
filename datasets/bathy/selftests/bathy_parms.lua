local runner = require("test_executive")
local console = require("console")
local asset = require("asset")
local prettyprint = require("prettyprint")

-- configure logging
console.monitor:config(core.LOG, core.INFO)
sys.setlvl(core.LOG, core.INFO)

-- load asset directory
local _assets = asset.loaddir()

-- get default parameters
local parms = bathy.parms()
local ptable = parms:export()

-- print defaults
prettyprint.display(ptable)

-- check some defaults
runner.check(ptable["beams"][1] == 'gt1l')
runner.check(ptable["classifiers"][5] == 'cshelph')
runner.check(ptable['timeout'] == 600)

-- check using direct access __index function
runner.check(parms["beams"][1] == 'gt1l')
runner.check(parms["classifiers"][5] == 'cshelph')
runner.check(parms['timeout'] == 600)

-- report results
runner.report()
