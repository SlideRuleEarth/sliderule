-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local http_code, size, response, status = cre.list()
    print(status, http_code, size, response)
    return response
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    name = "List Containers",
    description = "List available docker images that can be executed by a user",
    logging = core.CRITICAL,
    roles = {"member", "owner"},
    signed = true,
    outputs = {"json"}
}
