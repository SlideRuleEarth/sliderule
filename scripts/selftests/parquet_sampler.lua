local runner = require("test_executive")
console = require("console")
asset = require("asset")
local td = runner.rootdir(arg[0])

local in_file = td.."test.parquet"
local out_file = td.."samples.parquet"

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()


-- local dem1 = geo.raster(geo.parms({asset="arcticdem-strips", algorithm="NearestNeighbour", radius=0, with_flags=true}))
local dem1 = geo.raster(geo.parms({asset="arcticdem-mosaic", algorithm="NearestNeighbour", radius=30, zonal_stats=true}))
-- local dem1 = geo.raster(geo.parms({asset="arcticdem-mosaic", algorithm="NearestNeighbour", radius=0}))
runner.check(dem1 ~= nil)

local dem2 = geo.raster(geo.parms({asset="arcticdem-strips", algorithm="NearestNeighbour", radius=0, with_flags=true}))
runner.check(dem2 ~= nil)

local parquet_sampler = arrow.parquetsampler(in_file, out_file, {dem1, dem2})
runner.check(parquet_sampler ~= nil)

-- local parquet_sampler = arrow.parquetsampler(in_file, out_file, {dem1})
-- runner.check(parquet_sampler ~= nil)

local tstr = "2021:2:4:23:3:0"
-- local status = parquet_sampler:sample()
local status = parquet_sampler:sample(tstr)

-- Report Results --

runner.report()

