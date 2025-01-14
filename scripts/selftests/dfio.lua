local runner = require("test_executive")
local prettyprint = require("prettyprint")

-- Unit Test: DataFrame Import and Export --

do
    local table_in = {a = {1,2,3,4}, b = {11,12,13,14}, c = {21,22,23,24}}
    local meta_in = {bob = 100, bill = 200, cynthia = 300}
    local df = core.dataframe(table_in, meta_in)
    local df_out = df:export()
    local table_out = df_out["gdf"]
    local meta_out = df_out["meta"]

    for k,_ in pairs(table_in) do
        for i = 1,4 do
            runner.check(table_in[k][i] == df[k][i], string.format("dataframe mismatch on key %s, row %d: %d != %d", k, i, table_in[k][i], df[k][i]))
            runner.check(table_in[k][i] == table_out[k][i], string.format("exported table mismatch on key %s, row %d: %d != %d", k, i, table_in[k][i], table_out[k][i]))
        end
    end

    for k,_ in pairs(meta_in) do
        runner.check(meta_in[k] == meta_out[k], string.format("metadata mismatch on key %s: %f != %f", k, meta_in[k], meta_out[k]))
    end
end

-- Report Results --

runner.report()

