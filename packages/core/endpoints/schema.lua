--
-- ENDPOINT:    /source/schema
--
-- INPUT:       query string: api=<api_name>
--              e.g. /source/schema?api=atl06x
--              If no api parameter, returns list of available APIs.
--
-- OUTPUT:      JSON object with column definitions for the requested API
--
-- NOTES:       Reads schema definitions from JSON files on disk
--              ({confdir}/schemas/*.json) — no C++ registry needed.
--

local json = require("json")

---------------------------------------------------------------
-- Read a JSON file from disk
---------------------------------------------------------------
local function read_json(path)
    local f, err = io.open(path, "r")
    if not f then return nil, err end
    local text = f:read("*a")
    f:close()
    return json.decode(text)
end

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
local schemas_dir = __confdir .. "/schemas"
local params = parse_query(_rqst.arg)
local api = params["api"]

-- Load the index (used for both listing and validation)
local index = read_json(schemas_dir .. "/index.json")
if not index then
    return json.encode({error="failed to read schema index"})
end

if not api then
    -- Return listing of all registered schemas
    return json.encode({apis=index})
end

-- Validate api against the index to prevent path traversal
if not index[api] then
    return json.encode({error="unknown api: " .. api})
end

local schema = read_json(schemas_dir .. "/" .. api .. ".json")
if not schema then
    return json.encode({error="failed to read schema for: " .. api})
end

return json.encode(schema)
