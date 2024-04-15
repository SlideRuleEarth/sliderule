-- OLD H5CORO SCRIPT -- 

local runner = require("test_executive")
local console = require("console") -- non trivial printing
local td = runner.rootdir(arg[0]) -- set dir

-- Setup --

console.monitor:config(core.LOG, core.DEBUG) 
sys.setlvl(core.LOG, core.DEBUG)
asset = core.asset("local", "nil", "file", td, "empty.index")

-- Unit Test --

print('\n------------------\nTest: Read Dataset\n------------------')


dataq = "dataq" -- stream based: data comes in, then consumed/produced
rsps2 = msg.subscribe(dataq) -- responses posted to dataq

-- Arguments & Call --
local resource_path = "SNDR.J1.CRIS.20240414T2330.m06.g236.L1B.std.v02_11.G.240415051652.nc"
-- "h5ex_d_gzip.h5"
local dataset_name = "/lat_geoid/description"
-- "/minimum_fire_area/coordinates"
-- "/Temp/add_offset" --"/Power/_FillValue" -- "/Temp/add_offset" -- "/geospatial_lat_lon_extent/geospatial_lat_center"  -- "/Power/_FillValue" -- "/Power/valid_range" -- attributes on -> pass in attr -- "DS1" --
local id = 0
local raw = true
local dtype = core.INTEGER -- core.TEXT --core.INTEGER -- vs RecordObject::TEXT) RecordObject::REAL); RecordObject::INTEGER); RecordObject::DYNAMIC);
local col = 0
local startrow = 0
local numrows = core.ALL_ROWS

f2 = h5.dataset(core.READER, 
                asset, 
                resource_path, 
                dataset_name, 
                id, 
                raw, 
                dtype, 
                col, 
                startrow, 
                numrows)

-- reader for h5, h5 packages exposes dataset --> reader: needs asset, name, path, variable, row/col
r2 = core.reader(f2, dataq)

vals = rsps2:recvstring(3000) -- read out from listner

print("vals recieved with type: ".. type(vals))
print("len of vals:", #vals)
print("received string as:", vals)

e1 = string.unpack('I4', vals) -- ('f', vals) -- unpack values with float for FillRange

print("e1 val: " .. e1)
runner.check(400 == e1, "failed dataset read")
print("successful assert(s), destroying and closing")

-- print("e2 val: " ..e2)
-- print("e3 val: " ..e3)
-- print("e4 val: " ..e4)

rsps2:destroy()
r2:destroy()

-- Report Results --

runner.report()

