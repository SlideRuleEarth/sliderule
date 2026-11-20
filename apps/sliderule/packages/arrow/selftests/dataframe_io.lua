local runner = require("test_executive")
local _, dirpath = runner.srcscript()

-- Setup --

local in_parquet    = dirpath.."../../geo/data/atl06_10rows.parquet"
local in_geoparquet = dirpath.."../../geo/data/atl06_10rows.geoparquet"

local parms = core.parms({output={path="/tmp/arrow_dataframe_io.parquet", format="parquet"}})

-- Self Test --

runner.unittest("ArrowDataFrame Export and Import", function()

    local out_file = "/tmp/arrow_dataframe_io.parquet"
    local table_in = {a = {1.0, 2.0, 3.0, 4.0}, b = {11.0, 12.0, 13.0, 14.0}}
    local meta_in = {bob = 100, bill = 200}

    local df_in = core.dataframe(table_in, meta_in)
    runner.assert(arrow.dataframe(parms, df_in):export(out_file) == out_file, "failed to export dataframe", true)

    local df_out = core.dataframe()
    runner.assert(arrow.dataframe(parms, df_out):import(out_file), "failed to import dataframe", true)
    runner.assert(df_out:numrows() == 4, string.format("incorrect number of rows: %d", df_out:numrows()))
    runner.assert(df_out:numcols() == 2, string.format("incorrect number of columns: %d", df_out:numcols()))

    for k,_ in pairs(table_in) do
        for i = 1,4 do
            runner.assert(table_in[k][i] == df_out[k][i], string.format("column mismatch on key %s, row %d: %f != %f", k, i, table_in[k][i], df_out[k][i]))
        end
    end

    for k,_ in pairs(meta_in) do
        runner.assert(meta_in[k] == df_out:meta(k), string.format("metadata mismatch on key %s: %f != %f", k, meta_in[k], df_out:meta(k)))
    end

    os.remove(out_file)
end)

-- Self Test --

runner.unittest("ArrowDataFrame Import Parquet", function()

    local df = core.dataframe()
    runner.assert(arrow.dataframe(parms, df):import(in_parquet), "failed to import parquet file", true)
    runner.assert(df:numrows() == 10, string.format("incorrect number of rows: %d", df:numrows()))
    runner.assert(df:numcols() > 0, "no columns imported")
    runner.assert(df["longitude"][1] ~= nil, "missing longitude column")
    runner.assert(df["latitude"][1] ~= nil, "missing latitude column")
end)

-- Self Test --

runner.unittest("ArrowDataFrame Import GeoParquet", function()

    -- the same 10 rows written with the x and y columns encoded as a wkb geometry column
    local df_xy = core.dataframe()
    runner.assert(arrow.dataframe(parms, df_xy):import(in_parquet), "failed to import parquet file", true)

    local df_geo = core.dataframe()
    runner.assert(arrow.dataframe(parms, df_geo):import(in_geoparquet), "failed to import geoparquet file", true)
    runner.assert(df_geo:numrows() == df_xy:numrows(), string.format("incorrect number of rows: %d", df_geo:numrows()))

    for i = 1,df_xy:numrows() do
        runner.assert(math.abs(df_geo["longitude"][i] - df_xy["longitude"][i]) < 0.000001, string.format("longitude mismatch on row %d: %f != %f", i, df_geo["longitude"][i], df_xy["longitude"][i]))
        runner.assert(math.abs(df_geo["latitude"][i] - df_xy["latitude"][i]) < 0.000001, string.format("latitude mismatch on row %d: %f != %f", i, df_geo["latitude"][i], df_xy["latitude"][i]))
    end
end)
