local runner = require("test_executive")

-- Self Test: Schema Listing --

runner.unittest("Schema Listing", function()
    local apis = core.schema()
    runner.assert(type(apis) == "table", "core.schema() should return a table")
    local count = 0
    for _ in pairs(apis) do count = count + 1 end
    runner.assert(count > 0, "expected at least one registered schema")
end)

-- Self Test: Schema Detail --

runner.unittest("Schema Detail", function()
    local schema = core.schema("atl06x")
    runner.assert(type(schema) == "table", "schema detail should be a table")
    runner.assert(schema.description ~= nil, "description should be present")
    runner.assert(type(schema.columns) == "table", "schema should have columns")
    runner.assert(#schema.columns > 0, "schema should have at least one column")

    -- check column fields
    local col = schema.columns[1]
    runner.assert(col.name ~= nil, "column should have a name")
    runner.assert(col.type ~= nil, "column should have a type")
    runner.assert(col.role ~= nil, "column should have a role")
end)

-- Self Test: Column Count Spot Check --

runner.unittest("Schema Column Counts", function()
    -- atl06x: 19 columns + 5 metadata + srcid = 25
    local atl06 = core.schema("atl06x")
    runner.assert(#atl06.columns == 25, "atl06x should have 25 columns (got " .. #atl06.columns .. ")")

    -- casals1bx: 4 columns + 0 metadata + srcid = 5
    local casals = core.schema("casals1bx")
    runner.assert(#casals.columns == 5, "casals1bx should have 5 columns (got " .. #casals.columns .. ")")
end)

-- Self Test: Unknown API --

runner.unittest("Schema Unknown API", function()
    local ok, _ = pcall(core.schema, "nonexistent_api_xyz")
    runner.assert(not ok, "unknown api should raise an error")
end)

-- Report Results --

runner.report()
