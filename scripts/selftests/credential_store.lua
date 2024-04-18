local runner = require("test_executive")
local console = require("console")
local json = require("json")

-- Unit Test --

print('\n------------------\nTest01: Credential Storage\n------------------')

runner.check(aws.csput("mycredentials", {accessKeyId="1234", secretAccessKey="5678", sessionToken="abcdefg", expiration="2021-06-02 14:59:56+00:00"}), "failed to store my credentials")

local creds = aws.csget("mycredentials")

runner.check(creds.accessKeyId == "1234")
runner.check(creds.secretAccessKey == "5678")
runner.check(creds.sessionToken == "abcdefg")
runner.check(creds.expiration == "2021-06-02 14:59:56+00:00")

-- Clean Up --

server:destroy()
client:destroy()

-- Report Results --

runner.report()

