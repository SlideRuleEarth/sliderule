local runner = require("test_executive")

-- Self Test: Schema Registry --

runner.unittest("Schema Listing", function()
    local apis = core.schema()
    runner.assert(type(apis) == "table", "core.schema() should return a table")
    -- at least one schema should be registered (e.g. atl06x from icesat2)
    local count = 0
    for _ in pairs(apis) do count = count + 1 end
    runner.assert(count > 0, "expected at least one registered schema")
end)

runner.unittest("Schema Detail", function()
    local apis = core.schema()
    -- pick the first registered api
    local api_name = nil
    for k in pairs(apis) do api_name = k; break end
    runner.assert(api_name ~= nil, "need at least one schema to test detail")

    local schema = core.schema(api_name)
    runner.assert(type(schema) == "table", "schema detail should be a table")
    runner.assert(type(schema.description) == "string", "schema should have a description")
    runner.assert(type(schema.columns) == "table", "schema should have columns")
    runner.assert(#schema.columns > 0, "schema should have at least one column")

    -- check first column has expected fields
    local col = schema.columns[1]
    runner.assert(type(col.name) == "string", "column should have name")
    runner.assert(type(col.type) == "string", "column should have type")
    runner.assert(type(col.description) == "string", "column should have description")
    runner.assert(type(col.role) == "string", "column should have role")
    runner.assert(col.role == "column" or col.role == "element", "role should be column or element")
end)

runner.unittest("Schema Unknown API", function()
    local ok, _ = pcall(core.schema, "nonexistent_api_xyz")
    runner.assert(not ok, "unknown api should raise an error")
end)

-- Report Results --

runner.report()
