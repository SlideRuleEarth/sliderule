--
-- ENDPOINT:    /source/time
--
-- INPUT:       None
--
-- OUTPUT:      {
--                  "server":
--                  {
--                      "version": "1.0.1",                             # version
--                      "commit": "v1.0.1-4-g34f2782",                  # git commit description
--                      "launch": "2021:92:20:14:33",                   # YYYY:DOY:HH:DD:SS
--                      "duration": 304233,                             # ms since launch
--                      "packages": ["core", "aws", h5", "icesat2"],    # packages (and plugins) loaded
--                  },
--                  "icesat2":                                          # package/plugin version information when available
--                  {
--                      "version": "1.0.1",
--                      "commit": "v1.0.1-15-ga52359e",
--                  }
--              }
--
-- NOTES:       The output is a json object
--

local global = require("global")
local json = require("json")

version, commit, launch, duration, packages = sys.version()

rsps = {server={version=version, commit=commit, launch=launch, duration=duration, packages=packages}}

for _,package in ipairs(packages) do
    package_exists = global.check(package)
    if package_exists then
        version_function = global.eval("version", package)
        if version_function then
            package_version, package_commit = version_function()
            rsps[package] = {version=package_version, commit=package_commit}
        end
    end
end

return json.encode(rsps)
