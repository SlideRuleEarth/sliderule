local runner = require("test_executive")
local console = require("console")
local json = require("json")

local username = arg[1]
local password = arg[2]
local org_name = os.getenv("CLUSTER") or "local"
local url = os.getenv("PROVISIONING_SYSTEM") or "https://ps.testsliderule.org"

-- Set URL and Organization
netsvc.psurl(url)
netsvc.psorg(org_name)

-- Login
local rqst = {username=username, password=password, org_name=org_name}
local rsps = netsvc.get(url.."/ps/api/org_token/", true, true, json.encode(rqst))

-- Validate
local status = netsvc.psvalidate(rsps["access"], true)
runner.check(status, "unable to validate member")


runner.report()
