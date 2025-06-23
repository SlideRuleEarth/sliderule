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
local jsonfile = td .. "../data/rema_strips.json"
local contents = readgeojson.load(jsonfile)

-- Self Test --

local sigma = 1.0e-9

local lon = -80.0
local lat = -80.0
local height = 0

local demTypes = {"rema-mosaic", "rema-strips"}

expStripsResults = {
    {324.50000000, 0x0, "/vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s80w080/SETSM_s2s041_WV01_20241216_1020010108B4CB00_1020010108CCEC00_2m_seg1_dem.tif"},
    {328.64062500, 0x0, "/vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s80w081/SETSM_s2s041_WV01_20241214_10200101089DA900_1020010108DBDD00_2m_seg1_dem.tif"},
    {327.97656250, 0x0, "/vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s81w080/SETSM_s2s041_WV03_20241109_104001009D91AE00_104001009E0AD900_2m_seg1_dem.tif"},
    {326.41406250, 0x0, "/vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s81w081/SETSM_s2s041_WV01_20240116_10200100E61F4000_10200100E6692C00_2m_seg1_dem.tif"},
    {326.78125000, 0x0, "/vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s80w081/SETSM_s2s041_WV01_20221231_10200100D0B6E000_10200100D0E09200_2m_seg1_dem.tif"},
    {331.26562500, 0x0, "/vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s80w080/SETSM_s2s041_WV02_20221227_10300100DFD22B00_10300100E033EF00_2m_seg1_dem.tif"},
    {331.07812500, 0x0, "/vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s80w081/SETSM_s2s041_WV02_20221225_10300100DF52C000_10300100DFB54A00_2m_seg1_dem.tif"},
    {330.35156250, 0x0, "/vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s80w081/SETSM_s2s041_WV01_20221224_10200100D068CF00_10200100D1E98F00_2m_seg1_dem.tif"},
    {329.20312500, 0x0, "/vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s80w080/SETSM_s2s041_WV02_20221127_10300100DD6A9B00_10300100DDC36B00_2m_lsf_seg1_dem.tif"},
    {329.79687500, 0x0, "/vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s81w080/SETSM_s2s041_WV01_20220315_10200100C09FB700_10200100C24CF400_2m_lsf_seg1_dem.tif"},
    {329.85937500, 0x0, "/vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s81w081/SETSM_s2s041_WV01_20220116_10200100BE813000_10200100BE98FE00_2m_lsf_seg1_dem.tif"},
    {332.49218750, 0x0, "/vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s81w081/SETSM_s2s041_WV01_20211205_10200100BB4DF800_10200100BCE11800_2m_lsf_seg1_dem.tif"},
    {324.85156250, 0x0, "/vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s80w081/SETSM_s2s041_WV02_20211205_10300100CA0BF900_10300100CA2DB800_2m_lsf_seg1_dem.tif"},
    {329.32812500, 0x0, "/vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s81w081/SETSM_s2s041_WV01_20210211_10200100A2B8B100_10200100A5CD9500_2m_lsf_seg1_dem.tif"},
    {329.36718750, 0x0, "/vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s80w081/SETSM_s2s041_WV01_20210115_10200100A1CAC800_10200100A2221B00_2m_lsf_seg1_dem.tif"},
    {326.85156250, 0x0, "/vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s80w080/SETSM_s2s041_WV01_20210114_10200100A28A8100_10200100A331C000_2m_lsf_seg1_dem.tif"},
    {325.92187500, 0x0, "/vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s80w081/SETSM_s2s041_WV01_20210102_102001009F18B600_10200100A31C2700_2m_lsf_seg1_dem.tif"},
    {326.78125000, 0x0, "/vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s81w081/SETSM_s2s041_WV01_20201202_102001009ED42A00_102001009ED55800_2m_seg1_dem.tif"},
    {333.61718750, 0x0, "/vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s80w080/SETSM_s2s041_WV03_20201105_1040010062706C00_104001006337FF00_2m_lsf_seg1_dem.tif"},
    {330.85937500, 0x0, "/vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s81w080/SETSM_s2s041_WV02_20191229_103001009DD6F100_10300100A1347400_2m_lsf_seg1_dem.tif"},
    {327.67968750, 0x0, "/vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s81w081/SETSM_s2s041_WV01_20191027_102001008D725700_102001008A864B00_2m_lsf_seg1_dem.tif"},
    {325.99218750, 0x0, "/vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s80w081/SETSM_s2s041_WV02_20190207_103001008B6F7200_103001008B462E00_2m_lsf_seg1_dem.tif"},
    {326.29687500, 0x0, "/vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s80w080/SETSM_s2s041_WV03_20190124_1040010047040100_1040010046ADD200_2m_lsf_seg1_dem.tif"},
    {329.39062500, 0x0, "/vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s80w081/SETSM_s2s041_WV01_20190112_1020010082B90800_102001007E3E4400_2m_lsf_seg1_dem.tif"},
    {328.36718750, 0x0, "/vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s81w081/SETSM_s2s041_WV02_20180111_1030010075A80500_10300100769C3000_2m_lsf_seg1_dem.tif"},
    {327.44531250, 0x0, "/vsis3/pgc-opendata-dems/rema/strips/s2s041/2m/s81w080/SETSM_s2s041_WV01_20180102_102001006D62B700_102001006CDB0C00_2m_lsf_seg1_dem.tif"}
}
-- Correct values test for different POIs

for i = 1, #demTypes do
    local demType = demTypes[i];
    print(string.format("\n--------------------------------\nTest: %s Reading Correct Values\n--------------------------------", demType))
    dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", catalog=contents, radius=0, with_flags=true, sort_by_index=true}))

    local sampleCnt = 0
    tbl, err = dem:sample(lon, lat, height)
    if err ~= 0 then
        print(string.format("Point: %d, (%.3f, %.3f) ======> FAILED to read",j, lon, lat))
    else
        local el, fname
        for k, v in ipairs(tbl) do
            el = v["value"]
            fname = v["file"]
            flags = v["flags"]
            print(string.format("(%02d) %16.8fm  qmask: 0x%x %s", k, el, flags, fname))
            sampleCnt = sampleCnt + 1

            if demType == "rema-mosaic" then
                runner.assert(math.abs(el - 328.015625) < sigma)
            else
                runner.assert(math.abs(el - expStripsResults[k][1]) < sigma)
                runner.assert(flags == expStripsResults[k][2])
                runner.assert(fname == expStripsResults[k][3])
            end
        end
    end

    if demType == "rema-mosaic" then
        expectedSamplesCnt = 1
    else
        expectedSamplesCnt = #expStripsResults
        print("\n")
    end
    -- print(string.format("(%02d) value: %d  exp: %d", i, sampleCnt, expectedSamplesCnt))
    runner.assert(sampleCnt == expectedSamplesCnt)
end

-- Report Results --

runner.report()
