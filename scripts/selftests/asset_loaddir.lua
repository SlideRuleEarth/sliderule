local runner = require("test_executive")
console = require("console")
asset = require("asset")

-- Setup --

--console.monitor:config(core.LOG, core.DEBUG)
--sys.setlvl(core.LOG, core.DEBUG)

-- Unit Test --

local assets = asset.loaddir()

local nsidc_s3 = core.getbyname("nsidc-s3")
local not_there = core.getbyname("not_there")
local arcticdem_mosaic = core.getbyname("arcticdem-mosaic")
nsidc_s3 = core.getbyname("nsidc-s3")

runner.check(nsidc_s3)
runner.check(not_there == nil)
runner.check(arcticdem_mosaic)

local assets = asset.loaddir()

nsidc_s3 = core.getbyname("nsidc-s3")
not_there = core.getbyname("not_there")
arcticdem_mosaic = core.getbyname("arcticdem-mosaic")
nsidc_s3 = core.getbyname("nsidc-s3")

runner.check(nsidc_s3)
runner.check(not_there == nil)
runner.check(arcticdem_mosaic)

local assets = asset.loaddir()

nsidc_s3 = core.getbyname("nsidc-s3")
not_there = core.getbyname("not_there")
arcticdem_mosaic = core.getbyname("arcticdem-mosaic")
nsidc_s3 = core.getbyname("nsidc-s3")

runner.check(nsidc_s3)
runner.check(not_there == nil)
runner.check(arcticdem_mosaic)

-- Clean Up --

-- Report Results --

runner.report()

