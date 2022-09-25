local console = require("console")

local bucket    = arg[1] or "icesat2-sliderule"
local key       = arg[2] or "/config/netrc"
local auth      = arg[3] or "iam_role_auth"
local region    = arg[4] or "us-west-2"
local asset     = arg[5] or "iam-role"

local auth_script = core.script(auth):name("AuthScript")

print("waiting...")
sys.wait(3)

local response, _ = aws.s3get(bucket, key, region, asset)

print(response)
