--
-- ENDPOINT:    /source/h5x
--

local json = require("json")
local rqst = json.decode(arg[1])
local parms = h5coro.parms(rqst["parms"], rqst["key_space"], rqst["asset"], rqst["resource"])

-- helper function: error_to_client
local function error_to_client(message, fatal)
    local userlog = msg.publish(_rqst.rspq)
    userlog:alert(core.INFO, core.RTE_FAILURE, string.format("request <%s> for %s: %s", _rqst.rspq, parms["resource"], message))
    if fatal then error(message) end
end

-- check arrow parameters
if parms:witharrow() then error_to_client("must specify parquet output parameters", true) end

-- get h5 object
local h5obj = h5coro.object(parms["asset"], parms["resource"])
if not h5obj then error_to_client("failed to open resource", true) end

-- create dataframes for each group
local dfs = {}
for group in parms["groups"] do
    local df = h5coro.dataframe(parms, h5obj):join(parms["timeout"])
    if not df then error_to_client("failed to create dataframe") end
    if df:numrows() <= 0 or df:numcols() <= 0 then
        error_to_client(string.format("%s produced an empty dataframe", group), false)
    else
        table.insert(dfs, df)
    end
end

-- concatenate dataframes
local df = core.dataframe(dfs) --TODO: need to implement GeoDataFrame luaConcatenate that takes a list of GeoDataFrames and builds a single GeoDataFrame

-- TODO: also consider supporting setting the X, Y, Z with column names

-- create arrow dataframe
local arrow_df = arrow.dataframe(parms, df)
if not arrow_df then error_to_client("failed to create arrow dataframe", true) end

-- write dataframe to parquet file
local arrow_filename = arrow_df:export()
if not arrow_filename then error_to_client("failed to write dataframe", true) end

-- send parquet file to user
local status = core.send2user(arrow_filename, parms, _rqst.rspq)
if not status then error_to_client("failed to send dataframe", true) end
