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

-- check some defaults
runner.check(ptable["beams"][1] == 'gt1l')
runner.check(ptable["classifiers"][5] == 'cshelph')
runner.check(ptable['timeout'] == 600)

-- check using direct access __index function
runner.check(parms["beams"][1] == 'gt1l')
runner.check(parms["classifiers"][5] == 'cshelph')
runner.check(parms['timeout'] == 600)

-- initialize parameters
local rqst_parms = {
    ["asset"] = "icesat2",
    ["resource"] = "ATL03_20230213042035_08341807_006_02.h5",
    ["atl09_asset"] = "icesat2",
    ["use_bathy_mask"] = true,
    ["spots"] = {1, 2, 3, 4, 5, 6},
    ["classifiers"] = {"qtrees", "coastnet"},
    ["srt"] = -1,
    ["cnf"] = "atl03_not_considered",
    ["pass_invalid"] = true,
    ["timeout"] = 50000,
    ["correction"] = {
        ["use_water_ri_mask"] = true,
    },
    ["output"] = {
        ["path"] = "myfile.bin",
        ["format"] = "parquet",
        ["open_on_complete"] = false,
        ["as_geo"] = false,
        ["with_checksum"] = false
    }
}
parms = bathy.parms(rqst_parms)
ptable = parms:export()
prettyprint.display(ptable)

-- check some values
runner.check(ptable["beams"][1] == 'gt1l')
runner.check(ptable["classifiers"][1] == 'qtrees')
runner.check(ptable["classifiers"][2] == 'coastnet')
runner.check(ptable["classifiers"][3] == nil)
runner.check(ptable['timeout'] == 50000)
runner.check(ptable["output"]["path"] == "myfile.bin")
runner.check(ptable["output"]["format"] == "parquet")
runner.check(ptable["output"]["with_checksum"] == false)

-- check using direct access __index function
runner.check(parms["beams"][1] == 'gt1l')
runner.check(parms["classifiers"][1] == 'qtrees')
runner.check(parms["classifiers"][2] == 'coastnet')
runner.check(parms["classifiers"][3] == nil)
runner.check(parms['timeout'] == 50000)
runner.check(parms["output"]["path"] == "myfile.bin")
runner.check(parms["output"]["format"] == "parquet")
runner.check(parms["output"]["with_checksum"] == false)

-- check time fields
runner.check(parms["year"] == 2023)
runner.check(parms["month"] == 2)
runner.check(parms["day"] == 13)

-- report results
runner.report()
