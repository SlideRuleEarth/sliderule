local runner = require("test_executive")
local console = require("console")
local json = require("json")

-- Unit Test --

runner.check(aws.csput("mycredentials", {access_key_id="1234", secret_access_key="5678", access_token="abcdefg", expiration_time="2021-06-02 14:59:56+00:00"}), "failed to store my credentials")

creds = aws.csget("mycredentials")

runner.check(creds.access_key_id == "1234")
runner.check(creds.secret_access_key == "5678")
runner.check(creds.access_token == "abcdefg")
runner.check(creds.expiration_time == "2021-06-02 14:59:56+00:00")

-- Clean Up --

-- Report Results --

runner.report()

