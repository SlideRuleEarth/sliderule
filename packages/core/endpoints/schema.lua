--
-- ENDPOINT:    /source/schema
--
-- INPUT:       query string: api=<api_name>
--              e.g. /source/schema?api=atl06x
--              If no api parameter, returns list of available APIs.
--
-- OUTPUT:      JSON object with column definitions for the requested API
--
-- NOTES:       Column schemas are registered at plugin init from C++
--              DataFrame definitions (see GeoDataFrame::registerSchema).
--

local json = require("json")

---------------------------------------------------------------
-- Parse query string
---------------------------------------------------------------
local function parse_query(qs)
    local params = {}
    if qs then
        for k, v in qs:gmatch("([^&=]+)=([^&=]+)") do
            params[k] = v
        end
    end
    return params
end

---------------------------------------------------------------
-- Handle request
---------------------------------------------------------------
local params = parse_query(_rqst.arg)
local api = params["api"]

if not api then
    -- Return listing of all registered schemas
    local apis = core.schema()
    return json.encode({apis=apis})
end

local ok, schema = pcall(core.schema, api)
if not ok then
    return json.encode({error="unknown api: " .. api})
end

return json.encode(schema)
