local M = {}

function M.write_samples_to_file(tbl, filepath)
    local function serialize_value(v)
        if type(v) == "string" then
            return string.format("%q", v)  -- properly quote strings
        elseif type(v) == "number" or type(v) == "boolean" then
            return tostring(v)
        else
            return "nil"  -- unsupported types
        end
    end

    local function write_subtable(f, tbl, indent)
        indent = indent or 0
        local prefix = string.rep("  ", indent)
        f:write("{\n")

        for k, v in pairs(tbl) do
            local key = type(k) == "number" and string.format("[%d]", k) or string.format("[%q]", tostring(k))

            if type(v) == "table" then
                f:write(string.format("%s  %s = ", prefix, key))
                write_subtable(f, v, indent + 1)
                f:write(",\n")
            else
                f:write(string.format("%s  %s = %s,\n", prefix, key, serialize_value(v)))
            end
        end

        f:write(prefix .. "}")
    end

    local file, err = io.open(filepath, "w")
    assert(file, "Failed to open file: " .. (err or filepath))

    file:write("return ")
    write_subtable(file, tbl, 0)
    file:write("\n")

    file:close()
end

function M.load_results_table(gz_filepath)
    assert(gz_filepath:sub(-7) == ".lua.gz", "Expected a '.lua.gz' file")

    local lua_filepath = gz_filepath:sub(1, -4)  -- strip '.gz'
    local f = io.open(lua_filepath, "r")
    if not f then
        -- Decompress the .gz file
        local gzcheck = io.open(gz_filepath, "r")
        assert(gzcheck, "File not found: " .. gz_filepath)
        gzcheck:close()

        local status = os.execute(string.format("gzip -d -k '%s'", gz_filepath))
        assert(status == true or status == 0, "Failed to decompress: " .. gz_filepath)
    else
        f:close()
    end

    -- Load the Lua table
    return dofile(lua_filepath)
end

function M.compare_samples_tables(tbl1, tbl2)
    local mismatches = 0
    local total_compared = 0

    for i, sample1 in ipairs(tbl1) do
        local sample2 = tbl2[i]

        if sample1 == nil and sample2 == nil then
            print(string.format("Point %d: Both are nil", i))
        elseif sample1 == nil then
            print(string.format("Point %d: tbl1 is nil, tbl2 is present", i))
            mismatches = mismatches + 1
        elseif sample2 == nil then
            print(string.format("Point %d: tbl2 is nil, tbl1 is present", i))
            mismatches = mismatches + 1
        else
            for j, e1 in ipairs(sample1) do
                local e2 = sample2[j]
                if not e2 then
                    print(string.format("Point %d Sample %d: tbl2 is missing entry", i, j))
                    mismatches = mismatches + 1
                else
                    for k, v1 in pairs(e1) do
                        local v2 = e2[k]
                        local mismatch = false
                        if type(v1) == "number" and type(v2) == "number" then
                            mismatch = math.abs(v1 - v2) > 1e-6
                        else
                            mismatch = v1 ~= v2
                        end

                        if mismatch then
                            print(string.format(
                                "Point %d Sample %d: field '%s' mismatch: %s vs %s",
                                i, j, k, tostring(v1), tostring(v2)
                            ))
                            mismatches = mismatches + 1
                        end
                        total_compared = total_compared + 1
                    end
                end
            end
        end
    end

    print(string.format("\nComparison complete: %d mismatches out of %d fields", mismatches, total_compared))
    return mismatches
end

return M
