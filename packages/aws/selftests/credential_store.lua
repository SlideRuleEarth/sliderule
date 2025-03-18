local runner = require("test_executive")
local prettyprint = require("prettyprint")

-- Self Test --

print('\n------------------\nTest01: Credential Storage\n------------------')

runner.assert(aws.csput("mycredentials", {accessKeyId="1234", secretAccessKey="5678", sessionToken="abcdefg", expiration="2021-06-02 14:59:56+00:00"}), "failed to store my credentials")

local creds = aws.csget("mycredentials")

prettyprint.display(creds)

runner.assert(creds.accessKeyId == "1234")
runner.assert(creds.secretAccessKey == "5678")
runner.assert(creds.sessionToken == "abcdefg")
runner.assert(creds.expiration == "2021-06-02 14:59:56+00:00")

-- Report Results --

runner.report()

