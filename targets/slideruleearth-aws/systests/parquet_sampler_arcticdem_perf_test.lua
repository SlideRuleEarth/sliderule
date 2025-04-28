local runner = require("test_executive")
console = require("console")
asset = require("asset")
local assets = asset.loaddir()
local td = runner.rootdir(arg[0])

-- Setup --
-- console.monitor:config(core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local outq_name = "outq-luatest"


-- local in_parquet = '/data/arcticdem/alaska62k.parquet'
-- local in_parquet = '/data/arcticdem/alaska962k.parquet'
local in_parquet = '/data/arcticdem/alaska_2milionpoints.parquet'

-- Indicates local file system (no s3 or client)
local prefix = "file://"

local _out_parquet   = "/data/arcticdem/output/alaska.parquet"
local out_parquet    = prefix .. _out_parquet

-- console.monitor:config(core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

function getFileSize(filePath)
    local file = io.open(filePath, "rb")  -- 'rb' mode opens the file in binary mode
    if not file then
        print("Could not open file: " .. filePath .. " for reading\n")
        return nil
    end
    local fileSize = file:seek("end")  -- Go to the end of the file
    file:close()  -- Close the file
    return fileSize
end

-- local dem1 = geo.raster(geo.parms({asset="arcticdem-mosaic", algorithm="NearestNeighbour", radius=30, zonal_stats=true}))
-- runner.assert(dem1 ~= nil)

local dem2 = geo.raster(geo.parms({asset="arcticdem-strips", algorithm="NearestNeighbour", radius=0, with_flags=true, use_poi_time=true}))
runner.assert(dem2 ~= nil)


print('\n--------------------------------------\nTest01: input/output parquet (x, y)\n--------------------------------------')
local starttime = time.latch();
parquet_sampler = arrow.sampler(core.parms({output={path=out_parquet, format="parquet"}}), in_parquet, outq_name, {["strips"] = dem2})
runner.assert(parquet_sampler ~= nil)
status = parquet_sampler:waiton()
local stoptime = time.latch();
local dtime = stoptime - starttime

print(string.format("ExecTime: %f", dtime))


in_file_size = getFileSize(in_parquet);
print("Input  parquet file size: "  .. in_file_size .. " bytes")

status = parquet_sampler:waiton()
out_file_size = getFileSize(_out_parquet);
print("Output parquet file size: " .. out_file_size .. " bytes")
runner.assert(out_file_size > in_file_size, "Output file size is not greater than input file size: ")

-- There is no easy way to read parquet file in Lua, check the size of the output files
-- the files were tested with python scripts

-- Remove the output files
os.remove(_out_parquet)

-- Report Results --

runner.report()

