--
-- ENDPOINT:    /source/h5x
--

local json = require("json")
local rqst = json.decode(arg[1])
local parms = h5coro.parms(rqst["parms"], rqst["key_space"], rqst["asset"], rqst["resource"])
local timeout = parms["timeout"] * 1000
local dfq_name = "df.".._rqst.rspq
local resource = parms["resource"]
local groups = parms["groups"]

-- helper function: error_to_client
local function error_to_client(message, fatal)
    local userlog = msg.publish(_rqst.rspq)
    userlog:alert(core.INFO, core.RTE_FAILURE, string.format("request <%s> for %s: %s", _rqst.rspq, resource, message))
    if fatal then error(message) end
end

-- check arrow parameters
if parms:witharrow() then error_to_client("must specify parquet output parameters", true) end

-- get geo columns
local time_column = parms["time"]:length() > 0 and parms["time"] or nil
local x_column = parms["x"]:length() > 0 and parms["x"] or nil
local y_column = parms["y"]:length() > 0 and parms["y"] or nil
local z_column = parms["z"]:length() > 0 and parms["z"] or nil

-- get h5 object
local h5obj = h5coro.object(parms["asset"], resource)
if not h5obj then error_to_client("failed to open resource", true) end

-- create dataframes for each group
local dfs = {}
for _,group in ipairs(groups) do
    local df = h5coro.dataframe(parms, h5obj, group)
    if not df then error_to_client("failed to create dataframe", true) end
    df:geo(time_column, x_column, y_column, z_column) -- (optionally) set geo columns
    table.insert(dfs, df)
end

-- create dataframe to receive all group dataframes
local final_df = core.dataframe({}, {endpoint="h5x", request=json.encode(rqst)}) -- receiving dataframe
final_df:receive(dfq_name, _rqst.rspq, #groups, timeout)

-- join and serialize (send) each dataframe
for _,df in ipairs(dfs) do
    df:join(timeout)
    if df:numrows() > 0 and df:numcols() > 0 then
        if parms:withsamplers() then df:run(geo.framesampler(parms)) end -- execute sampler runner
        df:run(core.framesender(dfq_name, parms["key_space"], timeout))
        df:run(core.TERMINATE)
    end
end

-- wait for each to finish being serialized (sent)
for _,df in ipairs(dfs) do
    local status = df:finished(timeout, _rqst.rspq)
    if not status then error_to_client("timed out waiting for dataframe", true) end
end

-- wait for final dataframe to finish being reconstructed
if not final_df:waiton(timeout) then error_to_client("failed to build concatenated dataframe", true) end

-- check for final dataframe being empty
if final_df:numrows() <= 0 or final_df:numcols() <= 0 then error_to_client("produced an empty dataframe", true) end

-- create arrow dataframe
local arrow_df = arrow.dataframe(parms, final_df)
if not arrow_df then error_to_client("failed to create arrow dataframe", true) end

-- write dataframe to parquet file
local arrow_filename = arrow_df:export()
if not arrow_filename then error_to_client("failed to write dataframe", true) end

-- send parquet file to user
local status = core.send2user(arrow_filename, parms, _rqst.rspq)
if not status then error_to_client("failed to send dataframe", true) end
