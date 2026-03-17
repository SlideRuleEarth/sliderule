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
local userlog = msg.publish(_rqst.rspq)
local dfq = msg.publish(dfq_name)

-- alert helper function
local function status_to_client(lvl, code, message)
    userlog:alert(lvl, code, string.format("request <%s> for %s: %s", _rqst.rspq, resource, message))
    if code == core.RTE_FAILURE then error(message) end
end

-- check arrow parameters
if not parms:witharrow() then status_to_client(core.CRITICAL, core.RTE_FAILURE, "must specify parquet output parameters") end

-- get standard columns
local index_column = parms:length("index") > 0 and parms["index"] or nil
local time_column = parms:length("time") > 0 and parms["time"] or nil
local x_column = parms:length("x") > 0 and parms["x"] or nil
local y_column = parms:length("y") > 0 and parms["y"] or nil
local z_column = parms:length("z") > 0 and parms["z"] or nil

-- get h5 object
local h5obj = h5coro.object(parms["asset"], resource)
if not h5obj then status_to_client(core.CRITICAL, core.RTE_FAILURE, "failed to open resource") end

-- create dataframes for each group
local dfs = {}
for i,group in ipairs(groups) do
    local df = h5coro.dataframe(parms, h5obj, group, i)
    if not df then status_to_client(core.CRITICAL, core.RTE_FAILURE, string.format("failed to create dataframe for group %s", group)) end
    dfs[group] = df
end

-- create dataframe to receive all group dataframes
local final_df = core.dataframe({}, {endpoint="h5x", request=json.encode(rqst)}) -- receiving dataframe
final_df:receive(dfq_name, _rqst.rspq, #groups, timeout)

-- join and serialize (send) each dataframe
for group,df in pairs(dfs) do
    df:join(timeout)
    if df:numrows() > 0 and df:numcols() > 0 then
        df:geo(time_column, x_column, y_column, z_column) -- (optionally) set geo columns
        if parms:withsamplers() then df:run(geo.framesampler(parms)) end -- execute sampler runner
        df:run(core.framesender(dfq_name, parms["key_space"], timeout))
    end
    df:run(core.TERMINATE)
    status_to_client(core.INFO, core.RTE_STATUS, string.format("dataframe for group %s created with %d columns and %d rows", group, df:numcols(), df:numrows()))
end

-- wait for each to finish being serialized (sent)
for group,df in pairs(dfs) do
    local status = df:finished(timeout, _rqst.rspq)
    if not status then status_to_client(core.CRITICAL, core.RTE_FAILURE, string.format("timed out waiting for dataframe for group %s", group)) end
end

-- wait for final dataframe to finish being reconstructed
dfq:sendstring("") -- post termination to final df
if not final_df:waiton(timeout) then status_to_client(core.CRITICAL, core.RTE_FAILURE, "failed to build concatenated dataframe") end

-- check for final dataframe being empty
if final_df:numrows() <= 0 or final_df:numcols() <= 0 then status_to_client(core.CRITICAL, core.RTE_FAILURE, "produced an empty dataframe") end
status_to_client(core.INFO, core.RTE_STATUS, string.format("final dataframe constructed with %d columns and %d rows", final_df:numcols(), final_df:numrows()))

-- build index (if necessary)
if not index_column then
    index_column = "_index"
    final_df:buildindex(index_column)
end

-- create arrow dataframe
local arrow_df = arrow.dataframe(parms, final_df, index_column)
if not arrow_df then status_to_client(core.CRITICAL, core.RTE_FAILURE, "failed to create arrow dataframe") end

-- write dataframe to parquet file
local arrow_filename = arrow_df:export()
if not arrow_filename then status_to_client(core.CRITICAL, core.RTE_FAILURE, "failed to write dataframe") end

-- send parquet file to user
local status = core.send2user(arrow_filename, parms, _rqst.rspq)
if not status then status_to_client(core.CRITICAL, core.RTE_FAILURE, "failed to send dataframe") end
