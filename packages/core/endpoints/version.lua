-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    -- imports
    local global = require("global")
    local json = require("json")

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
    name = "Version",
    description = "Returns current version and configuration of server",
    logging = core.DEBUG,
    roles = {},
    signed = false,
    outputs = {"json"}
}


-- OUTPUT:
--  {
--      "server":
--      {
--          "version": "1.0.1",                             # version
--          "build": "v1.0.1-4-g34f2782",                   # sliderule repo git commit description
--          "environment": "v1.3.2-6-g122de4f",             # build repo git commit description
--          "launch": "2021:92:20:14:33",                   # YYYY:DOY:HH:DD:SS
--          "duration": 304233,                             # ms since launch
--          "packages": ["core", "aws", h5", "icesat2"],    # packages (and plugins) loaded
--      },
--      "icesat2":                                          # package version information when available
--      {
--          "version": "1.0.1",
--          "build": "v1.0.1-15-ga52359e",
--      }
--  }
