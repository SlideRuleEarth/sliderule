local runner = require("test_executive")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --
-- runner.log(core.DEBUG)

local _,td = runner.srcscript()
package.path = td .. "../utils/?.lua;" .. package.path

local readgeojson = require("readgeojson")
local jsonfile = td .. "../data/arcticdem_strips.json"
local contents = readgeojson.load(jsonfile)

-- Unit Test For url filter --

local sigma = 1.0e-9

local lon = -150.0
local lat =   70.0
local height = 0
local demType = "arcticdem-strips"

local expResults = {
    {120.36718750, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n69w150/SETSM_s2s041_WV02_20220313_10300100CF178100_10300100CF1C4700_2m_lsf_seg1_dem.tif"},
    {117.67968750, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w151/SETSM_s2s041_WV02_20210622_10300100C0877F00_10300100C0947A00_2m_lsf_seg1_dem.tif"},
    {116.13281250, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w150/SETSM_s2s041_WV01_20210327_10200100A69B7D00_10200100A9635700_2m_lsf_seg1_dem.tif"},
    {111.21093750, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n69w150/SETSM_s2s041_W1W1_20210322_10200100A6917600_10200100AF06EC00_2m_lsf_seg1_dem.tif"},
    {117.26562500, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w150/SETSM_s2s041_WV01_20200429_1020010095DF8A00_10200100977C2200_2m_lsf_seg1_dem.tif"},
    {117.71093750, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n69w150/SETSM_s2s041_WV01_20200312_1020010099356D00_102001009077A100_2m_lsf_seg1_dem.tif"},
    {116.50781250, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w150/SETSM_s2s041_WV02_20190902_103001009985DB00_1030010099709D00_2m_lsf_seg1_dem.tif"},
    {116.25000000, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n69w151/SETSM_s2s041_WV02_20190620_10300100954C9000_1030010093D59200_2m_lsf_seg1_dem.tif"},
    {117.15625000, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w151/SETSM_s2s041_WV01_20190330_1020010083A94600_1020010084A89500_2m_lsf_seg1_dem.tif"},
    {185.74218750, 0x4, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w151/SETSM_s2s041_WV02_20181022_103001008785EE00_1030010088B92E00_2m_lsf_seg1_dem.tif"},
    {115.96093750, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w151/SETSM_s2s041_WV01_20181006_102001007CBAD700_102001007E138E00_2m_lsf_seg1_dem.tif"},
    {264.40625000, 0x4, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n69w151/SETSM_s2s041_WV02_20171012_1030010072D75400_1030010072370600_2m_lsf_seg1_dem.tif"},
    {111.71093750, 0x4, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n69w151/SETSM_s2s041_WV01_20170710_1020010063D34400_102001006311E100_2m_seg2_dem.tif"},
    {117.50781250, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n69w150/SETSM_s2s041_WV02_20170601_103001006A488E00_103001006954F200_2m_lsf_seg1_dem.tif"},
    {387.46093750, 0x4, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w151/SETSM_s2s041_WV03_20170517_104001002CD87400_104001002E2FF800_2m_lsf_seg1_dem.tif"},
    {491.89843750, 0x4, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n69w151/SETSM_s2s041_WV02_20170502_1030010069621400_1030010068AAAF00_2m_lsf_seg1_dem.tif"},
    {113.66406250, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w151/SETSM_s2s041_WV01_20170424_1020010060E49500_1020010061B1C200_2m_seg1_dem.tif"},
    {118.76562500, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w150/SETSM_s2s041_WV02_20170416_10300100688AFB00_103001006675B500_2m_lsf_seg1_dem.tif"},
    {120.22656250, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n69w151/SETSM_s2s041_WV02_20160617_10300100589E0400_103001005650B000_2m_lsf_seg1_dem.tif"},
    {116.59375000, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w150/SETSM_s2s041_WV01_20160612_102001004D1F3100_1020010051550800_2m_lsf_seg1_dem.tif"},
    {121.41406250, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n69w150/SETSM_s2s041_WV02_20160607_10300100578F9400_10300100587BCE00_2m_lsf_seg1_dem.tif"},
    {118.22656250, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w150/SETSM_s2s041_WV02_20150518_10300100401A2000_1030010042B27000_2m_lsf_seg1_dem.tif"},
    {117.05468750, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n69w150/SETSM_s2s041_WV03_20150405_104001000A01E200_104001000A8A1D00_2m_lsf_seg1_dem.tif"},
    {118.60937500, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n69w151/SETSM_s2s041_WV03_20150330_10400100092E9A00_1040010009911300_2m_lsf_seg1_dem.tif"},
    {114.46875000, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w151/SETSM_s2s041_WV01_20150327_102001003953C000_102001003C734800_2m_seg2_dem.tif"},
    {117.23437500, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n69w151/SETSM_s2s041_WV02_20140614_10300100322F2D00_103001003363E600_2m_lsf_seg1_dem.tif"},
    {119.55468750, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w150/SETSM_s2s041_WV01_20140414_102001002C8BC400_102001002FCE3500_2m_lsf_seg1_dem.tif"},
    {115.97656250, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n69w151/SETSM_s2s041_WV01_20130213_102001002081C100_1020010020C63600_2m_lsf_seg1_dem.tif"},
    {226.21875000, 0x4, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n69w151/SETSM_s2s041_WV02_20130212_103001001F058300_1030010020330B00_2m_lsf_seg1_dem.tif"},
    {118.57812500, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n69w150/SETSM_s2s041_WV01_20121021_102001001D82D500_102001001F87AB00_2m_lsf_seg1_dem.tif"},
    {116.28125000, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w150/SETSM_s2s041_WV02_20120705_103001001922F300_10300100192B3100_2m_lsf_seg1_dem.tif"},
    {112.99218750, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n69w150/SETSM_s2s041_WV01_20120301_1020010019E59D00_102001001A427300_2m_lsf_seg1_dem.tif"},
    {116.42187500, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w150/SETSM_s2s041_WV01_20111012_1020010018CFEC00_1020010015325600_2m_seg1_dem.tif"},
    {130.38281250, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n69w150/SETSM_s2s041_W1W1_20110426_1020010013811600_1020010013E7D200_2m_lsf_seg5_dem.tif"},
    {112.67968750, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w151/SETSM_s2s041_W1W1_20110414_102001001233F900_1020010013811600_2m_lsf_seg8_dem.tif"},
    {114.13281250, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n69w150/SETSM_s2s041_W1W1_20110405_1020010011EB9000_10200100126E8A00_2m_lsf_seg3_dem.tif"},
    {112.46093750, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w150/SETSM_s2s041_W1W1_20110405_102001001233F900_10200100126E8A00_2m_seg3_dem.tif"}
}

--Nothing gets filtered since all rasters have vsis3 in their path name
local url = "vsis3"
print(string.format("\n--------------------------------\nTest: %s URL Filter: %s\n--------------------------------", demType, url))
local dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", catalog=contents, radius=0, with_flags=true, sort_by_index=true}))
tbl, err = dem:sample(lon, lat, height)
runner.assert(err == 0)
runner.assert(tbl ~= nil)

sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local flags = v["flags"]
    print(string.format("(%02d) %16.8fm  qmask: 0x%x %s", i, el, flags, fname))
    sampleCnt = sampleCnt + 1

    runner.assert(math.abs(el - expResults[i][1]) < sigma)
    runner.assert(flags == expResults[i][2])
    runner.assert(fname == expResults[i][3])
end
runner.assert(sampleCnt == #expResults)
dem=nil



url = "SETSM_s2s041_W1W1_20110405_102001001233F900_10200100126E8A00"
expResults = {{112.46093750, 0x0, "/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w150/SETSM_s2s041_W1W1_20110405_102001001233F900_10200100126E8A00_2m_seg3_dem.tif"}}

print(string.format("\n--------------------------------\nTest: %s URL Filter: %s\n--------------------------------", demType, url))
dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", catalog=contents, radius=0, with_flags=true, substr=url, sort_by_index=true}))
tbl, err = dem:sample(lon, lat, height)
runner.assert(err == 0)
runner.assert(tbl ~= nil)

sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local flags = v["flags"]
    print(string.format("(%02d) %16.8fm  qmask: 0x%x %s", i, el, flags, fname))
    sampleCnt = sampleCnt + 1

    runner.assert(math.abs(el - expResults[i][1]) < sigma)
    runner.assert(flags == expResults[i][2])
    runner.assert(fname == expResults[i][3])
end
runner.assert(sampleCnt == #expResults)
dem=nil

expResults = {
    {116.13281250, 0x0, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w150/SETSM_s2s041_WV01_20210327_10200100A69B7D00_10200100A9635700_2m_lsf_seg1_dem.tif'},
    {117.26562500, 0x0, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w150/SETSM_s2s041_WV01_20200429_1020010095DF8A00_10200100977C2200_2m_lsf_seg1_dem.tif'},
    {117.71093750, 0x0, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n69w150/SETSM_s2s041_WV01_20200312_1020010099356D00_102001009077A100_2m_lsf_seg1_dem.tif'},
    {117.15625000, 0x0, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w151/SETSM_s2s041_WV01_20190330_1020010083A94600_1020010084A89500_2m_lsf_seg1_dem.tif'},
    {115.96093750, 0x0, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w151/SETSM_s2s041_WV01_20181006_102001007CBAD700_102001007E138E00_2m_lsf_seg1_dem.tif'},
    {111.71093750, 0x4, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n69w151/SETSM_s2s041_WV01_20170710_1020010063D34400_102001006311E100_2m_seg2_dem.tif'},
    {113.66406250, 0x0, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w151/SETSM_s2s041_WV01_20170424_1020010060E49500_1020010061B1C200_2m_seg1_dem.tif'},
    {116.59375000, 0x0, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w150/SETSM_s2s041_WV01_20160612_102001004D1F3100_1020010051550800_2m_lsf_seg1_dem.tif'},
    {114.46875000, 0x0, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w151/SETSM_s2s041_WV01_20150327_102001003953C000_102001003C734800_2m_seg2_dem.tif'},
    {119.55468750, 0x0, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w150/SETSM_s2s041_WV01_20140414_102001002C8BC400_102001002FCE3500_2m_lsf_seg1_dem.tif'},
    {115.97656250, 0x0, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n69w151/SETSM_s2s041_WV01_20130213_102001002081C100_1020010020C63600_2m_lsf_seg1_dem.tif'},
    {118.57812500, 0x0, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n69w150/SETSM_s2s041_WV01_20121021_102001001D82D500_102001001F87AB00_2m_lsf_seg1_dem.tif'},
    {112.99218750, 0x0, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n69w150/SETSM_s2s041_WV01_20120301_1020010019E59D00_102001001A427300_2m_lsf_seg1_dem.tif'},
    {116.42187500, 0x0, '/vsis3/pgc-opendata-dems/arcticdem/strips/s2s041/2m/n70w150/SETSM_s2s041_WV01_20111012_1020010018CFEC00_1020010015325600_2m_seg1_dem.tif'}
}

url = "WV01"
print(string.format("\n--------------------------------\nTest: %s URL Filter: %s\n--------------------------------", demType, url))
dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", catalog=contents, radius=0, with_flags=true, substr=url, sort_by_index=true}))
tbl, err = dem:sample(lon, lat, height)
runner.assert(err == 0)
runner.assert(tbl ~= nil)

sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local flags = v["flags"]
    print(string.format("(%02d) %16.8fm  qmask: 0x%x %s", i, el, flags, fname))
    sampleCnt = sampleCnt + 1

    runner.assert(math.abs(el - expResults[i][1]) < sigma)
    runner.assert(flags == expResults[i][2])
    runner.assert(fname == expResults[i][3])
end
runner.assert(sampleCnt == #expResults)
dem=nil

-- Report Results --

runner.report()
