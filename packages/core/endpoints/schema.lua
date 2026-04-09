--
-- ENDPOINT:    /source/schema
--
-- INPUT:       query string: api=<api_name>
--              e.g. /source/schema?api=Atl06DataFrame
--              If no api parameter, returns list of available APIs.
--
-- OUTPUT:      JSON object with column definitions for the requested API
--

local json = require("json")

local function parse_query(qs)
    local params = {}
    if qs then
        for k, v in qs:gmatch("([^&=]+)=([^&=]+)") do
            params[k] = v
        end
    end
    return params
end

local params = parse_query(_rqst.arg)
local api = params["api"]

if not api then
    local apis = core.schema()
    return json.encode({apis=apis})
end

local schema = core.schema(api)
if not schema then
    return json.encode({error="unknown api: " .. api})
end

return json.encode(schema)
