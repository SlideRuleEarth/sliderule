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

    local dfq = msg.subscribe("dfq")
    local send_status = df_in:send("dfq")
    runner.check(send_status, "failed to send dataframe", true)
    local df = core.rxdataframe(dfq)

    prettyprint.display(df:export())

    for k,_ in pairs(table_in) do
        for i = 1,4 do
            runner.check(table_in[k][i] == df[k][i], string.format("dataframe mismatch on key %s, row %d: %d != %d", k, i, table_in[k][i], df[k][i]))
        end
    end

    for k,_ in pairs(meta_in) do
        runner.check(meta_in[k] == df:meta(k), string.format("metadata mismatch on key %s: %f != %f", k, meta_in[k], df:meta(k)))
    end
end)

-- Report Results --

runner.report()

