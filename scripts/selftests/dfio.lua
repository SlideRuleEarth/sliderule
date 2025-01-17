local runner = require("test_executive")
local prettyprint = require("prettyprint")

-- Configure Console

local console = require("console")
console.monitor:config(core.LOG, core.INFO)
sys.setlvl(core.LOG, core.INFO)

-- Unit Test --

runner.unittest("DataFrame Import and Export", function()

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

end)

-- Unit Test --

runner.unittest("DataFrame Send and Receive", function()

    local table_in = {a = {100,200,300,400}, b = {110,120,130,140}, c = {210,220,230,240}}
    local meta_in = {bob = 10, bill = 20, cynthia = 30}
    local df_in = core.dataframe(table_in, meta_in)
    local df_out = core.dataframe()

    df_out:receive("dfq", 1, "rspq") -- non-blocking
    runner.check(df_in:send("dfq"), "failed to send dataframe", true)
    runner.check(df_out:waiton(10000), "failed to receive dataframe", true)

    for k,_ in pairs(table_in) do
        for i = 1,4 do
            runner.check(table_in[k][i] == df_out[k][i], string.format("dataframe mismatch on key %s, row %d: %d != %d", k, i, table_in[k][i], df_out[k][i]))
        end
    end

    for k,_ in pairs(meta_in) do
        runner.check(meta_in[k] == df_out:meta(k), string.format("metadata mismatch on key %s: %f != %f", k, meta_in[k], df_out:meta(k)))
    end

end)

-- Report Results --

runner.report()

