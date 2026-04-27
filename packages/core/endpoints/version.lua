-------------------------------------------------------
-- initialization
-------------------------------------------------------
local global    = require("global")
local json      = require("json")
local parms     = nil

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()

    -- system version
    local version, build, launch, duration, packages = sys.version()

    -- build response
    local rsps = {
        server = {
            version=version,
            build=build,
            environment=sys.getcfg("environment_version"),
            launch=launch,
            duration=duration,
            cluster=sys.getcfg("cluster")
        }
    }

    -- add package version information
    for _,package in ipairs(packages) do
        local package_exists = global.check(package)
        if package_exists then
            local version_function = global.eval("version", package)
            if version_function then
                local package_version, package_build = version_function()
                if package ~= "sys" then
                    rsps[package] = {version=package_version, build=package_build}
                end
            end
        end
    end

    -- return response
    return json.encode(rsps)

end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "Version",
    description = "Returns current version and configuration of server",
    logging = core.DEBUG,
    roles = {},
    signed = false,
    outputs = {"json"}
}