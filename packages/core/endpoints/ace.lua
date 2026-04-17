-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    -- load user code
    local user_code = arg[1]
    local loaded_code, err = load(user_code)
    if not loaded_code then
        sys.log(core.CRITICAL, string.format("user code failed to execute with: %s", err))
        return string.format("unable to load user provided code: %s", err), false
    end

    -- execute and return results
    local result = loaded_code()
    return result
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    name = "Arbitrary Code Execution",
    description = "Execute user supplied lua code",
    logging = core.CRITICAL,
    roles = {"member", "owner"},
    signed = true,
    outputs = {"json"}
}
