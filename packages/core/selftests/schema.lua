local runner = require("test_executive")

-- Register a test schema to validate the mechanism
-- (Production schemas register dynamically when DataFrames are first constructed)
local COL = 0x8000 -- Field::NESTED_COLUMN

core.register_test_schema("__test__", "Test schema for selftest", {
    {name="col_a",  encoding=COL + 0x09, description="Test float column",    element=false},
    {name="col_b",  encoding=COL + 0x02, description="Test int32 column",    element=false},
    {name="meta_c", encoding=COL + 0x04, description="Test uint8 metadata",  element=true},
})

-- Self Test: Schema Registry --

runner.unittest("Schema Listing", function()
    local apis = core.schema()
    runner.assert(type(apis) == "table", "core.schema() should return a table")
    local count = 0
    for _ in pairs(apis) do count = count + 1 end
    runner.assert(count > 0, "expected at least one registered schema")
end)

runner.unittest("Schema Detail", function()
    local schema = core.schema("__test__")
    runner.assert(type(schema) == "table", "schema detail should be a table")
    runner.assert(schema.description == "Test schema for selftest", "description should match")
    runner.assert(type(schema.columns) == "table", "schema should have columns")
    runner.assert(#schema.columns == 3, "schema should have 3 columns")

    -- check column fields
    local col = schema.columns[1]
    runner.assert(col.name == "col_a", "first column name should be col_a")
    runner.assert(col.type == "float", "first column type should be float")
    runner.assert(col.description == "Test float column", "first column description should match")
    runner.assert(col.role == "column", "first column role should be column")

    -- check element field
    local meta = schema.columns[3]
    runner.assert(meta.name == "meta_c", "third field name should be meta_c")
    runner.assert(meta.role == "element", "third field role should be element")
end)

runner.unittest("Schema Unknown API", function()
    local ok, _ = pcall(core.schema, "nonexistent_api_xyz")
    runner.assert(not ok, "unknown api should raise an error")
end)

-- Report Results --

runner.report()
