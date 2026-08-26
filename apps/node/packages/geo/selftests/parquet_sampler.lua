local runner = require("test_executive")
local srcfile, dirpath = runner.srcscript()

-- Setup --

local timeout = 60 * 1000 -- 60 seconds

local in_geoparquet  = dirpath.."../data/atl06_10rows.geoparquet"
local in_parquet     = dirpath.."../data/atl06_10rows.parquet"

local geojsonfile = dirpath.."../data/arcticdem_strips.geojson"
local f = io.open(geojsonfile, "r")
runner.assert(f, "failed to open geojson file")
if not f then return end
local contents = f:read("*all")
f:close()

local function getFileSize(filePath)
    local file = io.open(filePath, "rb")  -- 'rb' mode opens the file in binary mode
    if not file then
        print("Could not open file: " .. filePath .. " for reading\n")
        return nil
    end
    local fileSize = file:seek("end")  -- Go to the end of the file
    file:close()  -- Close the file
    return fileSize
end

local function findMatchingString(filePath, searchTerm)
    local file = assert(io.open(filePath, "rb"))
    local data = file:read("*all")
    file:close()
    return string.find(data, searchTerm, 1, true)
end

local dem1 = {asset="arcticdem-mosaic", algorithm="NearestNeighbour", radius=30, zonal_stats=true, with_flags=true, slope_aspect=true, slope_scale_length=40}
local dem2 = {asset="arcticdem-strips", algorithm="NearestNeighbour", catalog=contents, radius=0, with_flags=true, slope_aspect=true, slope_scale_length=0, use_poi_time=true}

local function sampleGeoDataFrame (infile, outfile, format, samples)
    local parms = core.parms({output={format=format}, samples=samples})
    local gdf = core.dataframe()
    local adf = arrow.dataframe(parms, gdf):import(infile)
    runner.assert(adf, "failed to import dataframe", true)
    gdf:run(geo.framesampler(parms))
    gdf:run(core.TERMINATE)
    runner.assert(gdf:start(), "failed to start dataframe processing", true)
    runner.assert(gdf:finished(timeout), "failed to finish dataframe processing", true)
    adf:export(outfile)
end

-- Self Test --

runner.unittest("Input/Output GeoParquet (single)", function()
    local out_geoparquet = "/tmp/samples.geoparquet"
    sampleGeoDataFrame (in_geoparquet, out_geoparquet, "parquet", {mosaic=dem1})
    local out_file_size = getFileSize(out_geoparquet);
    runner.assert(out_file_size > 0, "Output file is empty")
    runner.assert(findMatchingString(out_geoparquet, "mosaic"), "Could not find key in output file")
    os.remove(out_geoparquet)
end)

runner.unittest("Input/Output GeoParquet (x, y)", function()
    local out_parquet = "/tmp/samples.parquet"
    sampleGeoDataFrame (in_parquet, out_parquet, "parquet", {mosaic=dem1})
    local out_file_size = getFileSize(out_parquet);
    runner.assert(out_file_size > 0, "Output file is empty")
    runner.assert(findMatchingString(out_parquet, "mosaic"), "Could not find key in output file")
    os.remove(out_parquet)
end)

runner.unittest("Input GeoParquet, Output Feather", function()
    local out_feather = "/tmp/samples.feather"
    sampleGeoDataFrame (in_geoparquet, out_feather, "feather", {mosaic=dem1})
    local in_file_size = getFileSize(in_geoparquet);
    print("Input geoparquet file size: "  .. in_file_size .. " bytes")
    local out_file_size = getFileSize(out_feather);
    print("Output parquet file size:    " .. out_file_size .. " bytes")
    runner.assert(out_file_size < in_file_size, "Output file size is not smaller than input file size: ")
    os.remove(out_feather)
end)

runner.unittest("Input Parquet, Output Feather", function()
    local out_feather = "/tmp/samples.feather"
    sampleGeoDataFrame (in_parquet, out_feather, "feather", {mosaic=dem1})
    local in_file_size = getFileSize(in_parquet);
    print("Input geoparquet file size: "  .. in_file_size .. " bytes")
    local out_file_size = getFileSize(out_feather);
    print("Output parquet file size:    " .. out_file_size .. " bytes")
    runner.assert(out_file_size < in_file_size, "Output file size is not smaller than input file size: ")
    os.remove(out_feather)
end)

runner.unittest("Input GeoParquet, Output CSV", function()
    local out_csv = "/tmp/samples.csv"
    sampleGeoDataFrame (in_geoparquet, out_csv, "csv", {mosaic=dem1})
    local in_file_size = getFileSize(in_geoparquet);
    print("Input geoparquet file size: " .. in_file_size .. " bytes")
    local out_file_size = getFileSize(out_csv);
    print("Output CSV file size:        " .. out_file_size .. " bytes")
    runner.assert(out_file_size < in_file_size, "Output CSV file size is not smaller than input file size: ")
    os.remove(out_csv)
end)

runner.unittest("Input Parquet, Output CSV", function()
    local out_csv = "/tmp/samples.csv"
    sampleGeoDataFrame (in_parquet, out_csv, "csv", {mosaic=dem1, strips=dem2})
    local in_file_size = getFileSize(in_parquet);
    print("Input parquet file size: "  .. in_file_size .. " bytes")
    local out_file_size = getFileSize(out_csv);
    print("Output CSV file size:     " .. out_file_size .. " bytes")
    runner.assert(out_file_size < in_file_size, "Output CSV file size is not smaller than input file size: ")
    os.remove(out_csv)
end)

runner.unittest("Input/Output GeoParquet (multiple)", function()
    local out_geoparquet = "/tmp/samples.geoparquet"
    sampleGeoDataFrame (in_geoparquet, out_geoparquet, "geoparquet", {mosaic=dem1, strips=dem2})
    local out_file_size = getFileSize(out_geoparquet);
    runner.assert(out_file_size > 0, "Output file is empty")
    runner.assert(findMatchingString(out_geoparquet, "mosaic"), "Could not find key in output file")
    runner.assert(findMatchingString(out_geoparquet, "strips"), "Could not find key in output file")
    os.remove(out_geoparquet)
end)

-- Report Results --

runner.report()
