local runner = require("test_executive")
local prettyprint = require("prettyprint")

-- Self Test --

runner.unittest("DataFrame Import and Export", function()

    local table_in = {a = {1,2,3,4}, b = {11,12,13,14}, c = {21,22,23,24}}
    local meta_in = {bob = 100, bill = 200, cynthia = 300}
    local df = core.dataframe(table_in, meta_in)
    local df_out = df:export()
    local table_out = df_out["gdf"]
    local meta_out = df_out["meta"]

    for k,_ in pairs(table_in) do
        for i = 1,4 do
            runner.assert(table_in[k][i] == df[k][i], string.format("dataframe mismatch on key %s, row %d: %d != %d", k, i, table_in[k][i], df[k][i]))
            runner.assert(table_in[k][i] == table_out[k][i], string.format("exported table mismatch on key %s, row %d: %d != %d", k, i, table_in[k][i], table_out[k][i]))
        end
    end

    for k,_ in pairs(meta_in) do
        runner.assert(meta_in[k] == meta_out[k], string.format("metadata mismatch on key %s: %f != %f", k, meta_in[k], meta_out[k]))
    end

end)

-- Self Test --

runner.unittest("DataFrame With CRS", function()

    local table_in = {x = {1.0, 2.0, 3.0}, y = {4.0, 5.0, 6.0}}
    local meta_in  = {description = "test frame"}
    local crs_in   = "EPSG:7912"

    -- Create dataframe with explicit CRS
    local df = core.dataframe(table_in, meta_in, crs_in)

    -- Check columns
    for i = 1,3 do
        runner.assert(table_in["x"][i] == df["x"][i], string.format("x mismatch row %d", i))
        runner.assert(table_in["y"][i] == df["y"][i], string.format("y mismatch row %d", i))
    end

    -- Export back out
    local df_out   = df:export()
    local meta_out = df_out["meta"]

    -- CRS should not be in meta, but accessible via getter
    runner.assert(df:crs() == crs_in, string.format("CRS mismatch: expected %s got %s", crs_in, tostring(df:crs())))
    runner.assert(meta_out["description"] == "test frame", "metadata field mismatch on description")
end)

-- Self Test --

runner.unittest("DataFrame Send and Receive", function()

    local table_in = {a = {100,200,300,400}, b = {110,120,130,140}, c = {210,220,230,240}}
    local meta_in = {bob = 10, bill = 20, cynthia = 30}
    local df_in = core.dataframe(table_in, meta_in)
    local df_out = core.dataframe()
    local dfq = msg.publish("dfq")

    df_out:receive("dfq", "rspq") -- non-blocking
    runner.assert(df_in:send("dfq"), "failed to send dataframe", true)
    dfq:sendstring("") -- terminator
    runner.assert(df_out:waiton(10000), "failed to receive dataframe", true)
    runner.assert(df_out:inerror() == false, "dataframe encountered error")

    for k,_ in pairs(table_in) do
        for i = 1,4 do
            runner.assert(table_in[k][i] == df_out[k][i], string.format("dataframe mismatch on key %s, row %d: %d != %d", k, i, table_in[k][i], df_out[k][i]))
        end
    end

    for k,_ in pairs(meta_in) do
        for i = 1,4 do
            runner.assert(meta_in[k] == df_out[k][i], string.format("metadata mismatch on key %s: %f != %f", k, meta_in[k], df_out[k][i]))
        end
    end

end)


-- Self Test --

runner.unittest("DataFrame Multiple Senders and Merged Receive", function()

    local table1_in = {a = {101,102,103,104}, b = {111,112,113,114}, c = {121,122,123,124}}
    local meta1_in = {bob = 11, bill = 12, cynthia = 13}
    local df1_in = core.dataframe(table1_in, meta1_in)
    local table2_in = {a = {201,202,203,204}, b = {211,212,213,214}, c = {221,222,223,224}}
    local meta2_in = {bob = 21, bill = 22, cynthia = 23}
    local df2_in = core.dataframe(table2_in, meta2_in)
    local df_out = core.dataframe()
    local dfq = msg.publish("dfq")

    df_out:receive("dfq", "rspq", 2) -- non-blocking
    runner.assert(df1_in:send("dfq", 0), "failed to send dataframe 1", true)
    runner.assert(df2_in:send("dfq", 1), "failed to send dataframe 2", true)
    dfq:sendstring("") -- terminator
    runner.assert(df_out:waiton(10000), "failed to receive dataframe", true)
    runner.assert(df_out:inerror() == false, "dataframe encountered error")

    prettyprint.display(df_out:export())

    for k,_ in pairs(table1_in) do
        for i = 1,4 do
            runner.assert(table1_in[k][i] == df_out[k][i], string.format("dataframe mismatch on key %s, row %d: %d != %d", k, i, table1_in[k][i], df_out[k][i]))
        end
        for i = 5,8 do
            runner.assert(table2_in[k][i-4] == df_out[k][i], string.format("dataframe mismatch on key %s, row %d: %d != %d", k, i, table2_in[k][i-4], df_out[k][i]))
        end
    end

    for k,_ in pairs(meta1_in) do
        for i = 1,4 do
            runner.assert(meta1_in[k] == df_out[k][i], string.format("dataframe mismatch on key %s, row %d: %d != %d", k, i, meta1_in[k], df_out[k][i]))
        end
        for i = 5,8 do
            runner.assert(meta2_in[k] == df_out[k][i], string.format("dataframe mismatch on key %s, row %d: %d != %d", k, i, meta1_in[k], df_out[k][i]))
        end
    end

end)

-- Self Test --

runner.unittest("DataFrame Multiple Senders With CRS", function()

    local table1_in = {a = {101,102,103,104}, b = {111,112,113,114}, c = {121,122,123,124}}
    local meta1_in  = {bob = 11, bill = 12, cynthia = 13}
    local df1_in    = core.dataframe(table1_in, meta1_in, "EPSG:7912")

    local table2_in = {a = {201,202,203,204}, b = {211,212,213,214}, c = {221,222,223,224}}
    local meta2_in  = {bob = 21, bill = 22, cynthia = 23}
    local df2_in    = core.dataframe(table2_in, meta2_in, "EPSG:7912")

    local df_out = core.dataframe()
    local dfq = msg.publish("dfq")

    df_out:receive("dfq", "rspq", 2) -- expect 2 senders
    runner.assert(df1_in:send("dfq", 0), "failed to send dataframe 1", true)
    runner.assert(df2_in:send("dfq", 1), "failed to send dataframe 2", true)
    dfq:sendstring("") -- terminator
    runner.assert(df_out:waiton(10000), "failed to receive dataframe", true)
    runner.assert(df_out:inerror() == false, "dataframe encountered error")

    -- Check merged columns
    for k,_ in pairs(table1_in) do
        for i = 1,4 do
            runner.assert(table1_in[k][i] == df_out[k][i],
                          string.format("dataframe mismatch on key %s, row %d", k, i))
        end
        for i = 5,8 do
            runner.assert(table2_in[k][i-4] == df_out[k][i],
                          string.format("dataframe mismatch on key %s, row %d", k, i))
        end
    end

    -- Check CRS consistency
    local crs = df_out:crs()
    runner.assert(crs == "EPSG:7912", string.format("CRS mismatch: expected EPSG:7912 got %s", tostring(crs)))
end)


-- Report Results --

runner.report()