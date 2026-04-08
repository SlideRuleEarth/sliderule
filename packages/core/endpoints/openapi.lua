--
-- ENDPOINT:    /source/openapi
--
-- INPUT:       none
--
-- OUTPUT:      Complete OpenAPI 3.1 spec as JSON with live column schemas
--              injected from the GeoDataFrame schema registry.
--
-- NOTES:       Reads the base spec template from {confdir}/openapi-base.json,
--              then injects column schemas from core.schema() into
--              components.schemas before returning the complete spec.
--

local json = require("json")

---------------------------------------------------------------
-- Type mapping: schema registry types -> OpenAPI property defs
---------------------------------------------------------------
local function schema_type_to_openapi(typ)
    -- Map types returned by core.schema() to OpenAPI type/format
    local map = {
        ["bool"]           = {type="boolean"},
        ["int8"]           = {type="integer", format="int8"},
        ["int16"]          = {type="integer", format="int16"},
        ["int32"]          = {type="integer", format="int32"},
        ["int64"]          = {type="integer", format="int64"},
        ["uint8"]          = {type="integer", format="uint8"},
        ["uint16"]         = {type="integer", format="uint16"},
        ["uint32"]         = {type="integer", format="uint32"},
        ["uint64"]         = {type="integer", format="uint64"},
        ["float"]          = {type="number",  format="float"},
        ["double"]         = {type="number",  format="double"},
        ["timestamp[ns]"]  = {type="string",  format="timestamp-ns"},
        ["string"]         = {type="string"},
    }

    local m = map[typ]
    if m then return m end

    -- Handle array<T> and list<T>
    local inner = typ:match("^array<(.+)>$") or typ:match("^list<(.+)>$")
    if inner then
        local item = map[inner] or {type="string"}
        return {type="array", items=item}
    end

    return {type="string", description="unknown type: " .. typ}
end

---------------------------------------------------------------
-- Build response column schemas from the live registry
---------------------------------------------------------------
local function build_column_schemas()
    local result = {}
    local apis = core.schema()

    for api_name, api_desc in pairs(apis) do
        local schema = core.schema(api_name)
        local properties = {}
        for _, col in ipairs(schema.columns) do
            local prop = schema_type_to_openapi(col.type)
            prop.description = col.description
            if col.role == "element" then
                prop.description = (prop.description or "") .. " (per-batch metadata)"
            end
            properties[col.name] = prop
        end

        -- Build a schema name from the api name (e.g. atl06x -> atl06x_columns)
        local schema_key = api_name .. "_columns"
        result[schema_key] = {
            type = "object",
            description = api_desc .. " — auto-generated from schema registry",
            properties = properties
        }
    end

    return result
end

---------------------------------------------------------------
-- Read the base spec template
---------------------------------------------------------------
local base_path = __confdir .. "/openapi-base.json"
local f, err = io.open(base_path, "r")
if not f then
    return json.encode({error = "failed to read base spec: " .. (err or "unknown")}), false
end
local base_text = f:read("*a")
f:close()

local ok, base_spec = pcall(json.decode, base_text)
if not ok then
    return json.encode({error = "failed to parse base spec: " .. tostring(base_spec)}), false
end

---------------------------------------------------------------
-- Inject live column schemas
---------------------------------------------------------------
if not base_spec.components then
    base_spec.components = {}
end
if not base_spec.components.schemas then
    base_spec.components.schemas = {}
end

local column_schemas = build_column_schemas()
for k, v in pairs(column_schemas) do
    base_spec.components.schemas[k] = v
end

---------------------------------------------------------------
-- Inject the API listing into the /source/schema enum
---------------------------------------------------------------
local apis = core.schema()
local api_list = {}
for name in pairs(apis) do
    api_list[#api_list + 1] = name
end
table.sort(api_list)

-- Add version info
local version, build = sys.version()
if base_spec.info then
    base_spec.info.version = version
end

---------------------------------------------------------------
-- Return the complete spec
---------------------------------------------------------------
return json.encode(base_spec)
