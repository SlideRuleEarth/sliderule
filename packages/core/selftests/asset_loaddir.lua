local runner = require("test_executive")
local console = require("console")
local asset = require("asset")

-- Setup --

--console.monitor:config(core.LOG, core.DEBUG)
--sys.setlvl(core.LOG, core.DEBUG)

-- Unit Test --

local assets = asset.loaddir()

local nsidc_s3 = core.getbyname("icesat2")
local not_there = core.getbyname("not_there")
local arcticdem_mosaic = core.getbyname("arcticdem-mosaic")
nsidc_s3 = core.getbyname("icesat2")

runner.assert(nsidc_s3)
runner.assert(not_there == nil)
runner.assert(arcticdem_mosaic)

local assets = asset.loaddir()

nsidc_s3 = core.getbyname("icesat2")
not_there = core.getbyname("not_there")
arcticdem_mosaic = core.getbyname("arcticdem-mosaic")
nsidc_s3 = core.getbyname("icesat2")

runner.assert(nsidc_s3)
runner.assert(not_there == nil)
runner.assert(arcticdem_mosaic)

local assets = asset.loaddir()

nsidc_s3 = core.getbyname("icesat2")
not_there = core.getbyname("not_there")
arcticdem_mosaic = core.getbyname("arcticdem-mosaic")
nsidc_s3 = core.getbyname("icesat2")

runner.assert(nsidc_s3)
runner.assert(not_there == nil)
runner.assert(arcticdem_mosaic)

-- Clean Up --

-- Report Results --

runner.report()

