local runner = require("test_executive")
local json = require("json")

-- Helper: read a JSON file from disk
local function read_json(path)
    local f = io.open(path, "r")
    if not f then return nil end
    local text = f:read("*a")
    f:close()
    local ok, result = pcall(json.decode, text)
    if ok then return result end
    return nil
end

local schemas_dir = __confdir .. "/schemas"

-- Self Test: Schema Index --

runner.unittest("Schema Index", function()
    local index = read_json(schemas_dir .. "/index.json")
    runner.assert(index ~= nil, "index.json should exist and be valid JSON")

    local count = 0
    for _ in pairs(index) do count = count + 1 end
    runner.assert(count >= 9, "expected at least 9 APIs in index (got " .. count .. ")")

    -- Spot-check a known API
    runner.assert(index["atl06x"] ~= nil, "index should contain atl06x")
end)

-- Self Test: Schema File Structure --

runner.unittest("Schema File Structure", function()
    local schema = read_json(schemas_dir .. "/atl06x.json")
    runner.assert(schema ~= nil, "atl06x.json should exist and be valid JSON")
    runner.assert(schema.api == "atl06x", "api field should be atl06x")
    runner.assert(type(schema.description) == "string", "description should be a string")
    runner.assert(type(schema.columns) == "table", "columns should be a table")
    runner.assert(#schema.columns > 0, "should have at least one column")

    -- Check first column has required fields
    local col = schema.columns[1]
    runner.assert(col.name ~= nil, "column should have a name")
    runner.assert(col.type ~= nil, "column should have a type")
    runner.assert(col.desc ~= nil, "column should have a desc")
end)

-- Self Test: All Index Entries Have Files --

runner.unittest("All Index Entries Have Files", function()
    local index = read_json(schemas_dir .. "/index.json")
    runner.assert(index ~= nil, "index.json should exist")

    for api_name in pairs(index) do
        local schema = read_json(schemas_dir .. "/" .. api_name .. ".json")
        runner.assert(schema ~= nil, api_name .. ".json should exist")
        runner.assert(schema.api == api_name, api_name .. " api field should match filename")
    end
end)

-- Self Test: Unknown API --

runner.unittest("Unknown Schema File", function()
    local schema = read_json(schemas_dir .. "/nonexistent_api_xyz.json")
    runner.assert(schema == nil, "nonexistent schema should return nil")
end)

-- Report Results --

runner.report()
