local runner = require("test_executive")
local console = require("console")
local json = require("json")

local username = arg[1]
local password = arg[2]
local org_name = arg[3] or "sliderule"
local url = os.getenv("PROVISIONING_SYSTEM") or "https://ps.testsliderule.org"

-- Set URL and Organization
core.psurl(url)
core.psorg(org_name)

-- Login
local rsps_str = core.pslogin(username, password, org_name, true)
local rsps = json.decode(rsps_str)
runner.assert(rsps["access"] ~= nil, "unable to login")

-- Validate
local status = core.psvalidate(rsps["access"], true)
runner.assert(status, "unable to validate member")


runner.report()
