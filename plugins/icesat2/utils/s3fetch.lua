local console = require("console")

local bucket = arg[1] or "icesat2-sliderule"
local key = arg[2] or "/config/netrc"

local auth_script = core.script("iam_role_auth"):name("AuthScript")
sys.wait(2)

local response, status = aws.s3curlget("icesat2-sliderule", "/config/netrc")
