local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()

-- Unit Test For Temporal filter --

local sigma = 1.0e-9

local lon = -178.0
local lat =   51.7
local height = 0

local demType = "arcticdem-strips"

local expResults = {{452.48437500, 0x4, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n51w178/SETSM_s2s041_WV01_20181210_102001007A560E00_10200100802C2300_2m_lsf_seg3_dem.tif'},
                    {632.90625000, 0x4, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n51w178/SETSM_s2s041_WV01_20200222_1020010099A56800_1020010095159800_2m_lsf_seg1_dem.tif'},
                    { 81.50000000, 0x0, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n51w178/SETSM_s2s041_WV01_20210127_10200100A0B3CA00_10200100A2906A00_2m_lsf_seg1_dem.tif'},
                    { 80.65625000, 0x0, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n51w178/SETSM_s2s041_WV01_20210303_10200100A47BBC00_10200100A64F5900_2m_lsf_seg1_dem.tif'},
                    { 82.89843750, 0x0, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n51w178/SETSM_s2s041_WV01_20210605_10200100B27E2100_10200100B2D61C00_2m_lsf_seg1_dem.tif'},
                    {773.03906250, 0x4, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n51w178/SETSM_s2s041_WV02_20140226_103001002D248E00_103001002E2D7D00_2m_lsf_seg1_dem.tif'},
                    { 80.22656250, 0x0, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n51w178/SETSM_s2s041_WV02_20160602_1030010057849C00_103001005607CA00_2m_lsf_seg1_dem.tif'},
                    { 83.57031250, 0x0, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n51w178/SETSM_s2s041_WV02_20170916_103001006F1E7900_10300100701E7200_2m_lsf_seg1_dem.tif'},
                    { 85.17187500, 0x0, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n51w178/SETSM_s2s041_WV02_20181220_103001008ABD1400_103001008A640D00_2m_lsf_seg1_dem.tif'},
                    { 83.91406250, 0x4, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n51w178/SETSM_s2s041_WV02_20200225_10300100A18EF500_10300100A32D9C00_2m_lsf_seg2_dem.tif'},
                    { 81.21093750, 0x0, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n51w178/SETSM_s2s041_WV02_20200326_10300100A60E3900_10300100A4BA7E00_2m_lsf_seg1_dem.tif'},
                    {117.30468750, 0x4, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n51w178/SETSM_s2s041_WV02_20210223_10300100B2CBC000_10300100B642EA00_2m_lsf_seg1_dem.tif'},
                    { 79.74218750, 0x0, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n51w178/SETSM_s2s041_WV03_20190906_104001005146B800_1040010051638000_2m_lsf_seg1_dem.tif'},
                    { 49.92187500, 0x4, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n51w178/SETSM_s2s041_WV03_20210204_10400100656B9F00_1040010065903500_2m_lsf_seg1_dem.tif'}}

--Nothing gets filtered since all rasters have vsis3 in their path name
local url = "vsis3"
print(string.format("\n--------------------------------\nTest: %s URL Filter: %s\n--------------------------------", demType, url))
local dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=0, with_flags=true}))
tbl, status = dem:sample(lon, lat, height)
runner.check(status == true)
runner.check(tbl ~= nil)

sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local flags = v["flags"]
    print(string.format("(%02d) %16.8fm  qmask: 0x%x %s", i, el, flags, fname))
    sampleCnt = sampleCnt + 1

    runner.check(math.abs(el - expResults[i][1]) < sigma)
    runner.check(flags == expResults[i][2])
    runner.check(fname == expResults[i][3])
end
runner.check(sampleCnt == #expResults)
dem=nil



url = "SETSM_s2s041_WV01_20181210_102001007A560E00_10200100802C2300"
expResults = {{452.48437500, 0x4, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n51w178/SETSM_s2s041_WV01_20181210_102001007A560E00_10200100802C2300_2m_lsf_seg3_dem.tif'}}

print(string.format("\n--------------------------------\nTest: %s URL Filter: %s\n--------------------------------", demType, url))
dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=0, with_flags=true, substr=url}))
tbl, status = dem:sample(lon, lat, height)
runner.check(status == true)
runner.check(tbl ~= nil)

sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local flags = v["flags"]
    print(string.format("(%02d) %16.8fm  qmask: 0x%x %s", i, el, flags, fname))
    sampleCnt = sampleCnt + 1

    runner.check(math.abs(el - expResults[i][1]) < sigma)
    runner.check(flags == expResults[i][2])
    runner.check(fname == expResults[i][3])
end
runner.check(sampleCnt == #expResults)
dem=nil



expResults = {{452.48437500, 0x4, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n51w178/SETSM_s2s041_WV01_20181210_102001007A560E00_10200100802C2300_2m_lsf_seg3_dem.tif'},
              {632.90625000, 0x4, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n51w178/SETSM_s2s041_WV01_20200222_1020010099A56800_1020010095159800_2m_lsf_seg1_dem.tif'},
              { 81.50000000, 0x0, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n51w178/SETSM_s2s041_WV01_20210127_10200100A0B3CA00_10200100A2906A00_2m_lsf_seg1_dem.tif'},
              { 80.65625000, 0x0, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n51w178/SETSM_s2s041_WV01_20210303_10200100A47BBC00_10200100A64F5900_2m_lsf_seg1_dem.tif'},
              { 82.89843750, 0x0, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n51w178/SETSM_s2s041_WV01_20210605_10200100B27E2100_10200100B2D61C00_2m_lsf_seg1_dem.tif'}}

url = "WV01"
print(string.format("\n--------------------------------\nTest: %s URL Filter: %s\n--------------------------------", demType, url))
dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=0, with_flags=true, substr=url}))
tbl, status = dem:sample(lon, lat, height)
runner.check(status == true)
runner.check(tbl ~= nil)

sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local flags = v["flags"]
    print(string.format("(%02d) %16.8fm  qmask: 0x%x %s", i, el, flags, fname))
    sampleCnt = sampleCnt + 1

    runner.check(math.abs(el - expResults[i][1]) < sigma)
    runner.check(flags == expResults[i][2])
    runner.check(fname == expResults[i][3])
end
runner.check(sampleCnt == #expResults)
dem=nil



-- Report Results --

runner.report()

