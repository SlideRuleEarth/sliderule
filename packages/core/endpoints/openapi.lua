--
-- ENDPOINT:    /source/openapi
--
-- INPUT:       none
--
-- OUTPUT:      Complete OpenAPI 3.1 spec as JSON with column schemas
--              injected from JSON schema files on disk.
--
-- NOTES:       Reads the base spec template from {confdir}/openapi-base.json,
--              then injects column schemas from {confdir}/schemas/*.json into
--              components.schemas before returning the complete spec.
--

local json = require("json")

---------------------------------------------------------------
-- Type mapping: C++ type strings -> OpenAPI property defs
---------------------------------------------------------------
local type_map = {
    ["bool"]      = {type="boolean"},
    ["int8_t"]    = {type="integer", format="int8"},
    ["int16_t"]   = {type="integer", format="int16"},
    ["int32_t"]   = {type="integer", format="int32"},
    ["int64_t"]   = {type="integer", format="int64"},
    ["uint8_t"]   = {type="integer", format="uint8"},
    ["uint16_t"]  = {type="integer", format="uint16"},
    ["uint32_t"]  = {type="integer", format="uint32"},
    ["uint64_t"]  = {type="integer", format="uint64"},
    ["float"]     = {type="number",  format="float"},
    ["double"]    = {type="number",  format="double"},
    ["time8_t"]   = {type="string",  format="timestamp-ns"},
    ["string"]    = {type="string"},
}

local function ctype_to_openapi(ctype)
    local m = type_map[ctype]
    if m then return m end

    -- Handle FieldArray<T,...> and FieldList<T>
    local inner = ctype:match("^FieldArray<(.+),") or ctype:match("^FieldList<(.+)>$")
    if inner then
        local item = type_map[inner] or {type="string"}
        return {type="array", items=item}
    end

    return {type="string", description="unknown type: " .. ctype}
end

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
-- Build response column schemas from JSON schema files
---------------------------------------------------------------
local function build_column_schemas()
    local result = {}
    local schemas_dir = __confdir .. "/schemas"

    -- Read index to enumerate all APIs
    local index = read_json(schemas_dir .. "/index.json")
    if not index then return result end

    for api_name, api_desc in pairs(index) do
        local schema = read_json(schemas_dir .. "/" .. api_name .. ".json")
        if schema then
            local properties = {}

            -- Add fixed columns
            if schema.columns then
                for _, col in ipairs(schema.columns) do
                    local prop = ctype_to_openapi(col.type)
                    prop.description = col.desc
                    properties[col.name] = prop
                end
            end

            -- Add staged columns
            if schema.staged then
                for _, stage in ipairs(schema.staged) do
                    for _, col in ipairs(stage.columns) do
                        local prop = ctype_to_openapi(col.type)
                        prop.description = col.desc .. " (conditional)"
                        properties[col.name] = prop
                    end
                end
            end

            -- Add metadata fields
            if schema.metadata then
                for _, meta in ipairs(schema.metadata) do
                    local prop = ctype_to_openapi(meta.type)
                    prop.description = (meta.desc or "") .. " (per-batch metadata)"
                    properties[meta.name] = prop
                end
            end

            -- Add srcid (always present)
            properties["srcid"] = {type="integer", format="int32", description="Source granule ID (see metadata)"}

            local schema_key = api_name .. "_columns"
            result[schema_key] = {
                type = "object",
                description = api_desc .. " — auto-generated from JSON schema",
                properties = properties
            }
        end
    end

    return result
end

---------------------------------------------------------------
-- Read the base spec template
---------------------------------------------------------------
local base_path = __confdir .. "/openapi-base.json"
local base_spec, err = read_json(base_path)
if not base_spec then
    return json.encode({error = "failed to read base spec: " .. (err or "unknown")}), false
end

---------------------------------------------------------------
-- Inject column schemas
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
-- Add version info
---------------------------------------------------------------
local version, build = sys.version()
if base_spec.info then
    base_spec.info.version = version
end

---------------------------------------------------------------
-- Return the complete spec
---------------------------------------------------------------
return json.encode(base_spec)
