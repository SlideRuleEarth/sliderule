local runner = require("test_executive")
local console = require("console")
local json = require("json")

local username = arg[1]
local password = arg[2]
local org_name = arg[3] or "sliderule"
local url = os.getenv("PROVISIONING_SYSTEM") or "https://ps.testsliderule.org"

-- Set URL and Organization
netsvc.psurl(url)
netsvc.psorg(org_name)

-- Login
local rsps_str = netsvc.pslogin(username, password, org_name, true)
local rsps = json.decode(rsps_str)
--runner.check(rsps["access"], "unable to login")

--netsvc.psurl("http://ps.testsliderule.org")

-- Validate
local status = netsvc.psvalidate(rsps["access"], true)
runner.check(status, "unable to validate member")


runner.report()
