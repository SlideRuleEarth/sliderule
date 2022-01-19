local runner = require("test_executive")
local console = require("console")
local json = require("json")

-- Unit Test --

print('\n------------------\nTest01: Credential Storage\n------------------')

runner.check(aws.csput("mycredentials", {accessKeyId="1234", secretAccessKey="5678", sessionToken="abcdefg", expiration="2021-06-02 14:59:56+00:00"}), "failed to store my credentials")

creds = aws.csget("mycredentials")

runner.check(creds.accessKeyId == "1234")
runner.check(creds.secretAccessKey == "5678")
runner.check(creds.sessionToken == "abcdefg")
runner.check(creds.expiration == "2021-06-02 14:59:56+00:00")

print('\n------------------\nTest01: Credential Metrics\n------------------')

server = core.httpd(9081)
endpoint = core.endpoint()
server:attach(endpoint, "/source")

client = core.http("127.0.0.1", 9081)

rsps = client:request("GET", "/source/version", "{}")
metrics = sys.metric("CredentialStore")
display = json.encode(metrics)
print(display)

runner.check(metrics["CredentialStore.mycredentials:exp_gps"]["value"] == 1306681214000, "incorrect expiration time")

-- Clean Up --

server:destroy()
client:destroy()

-- Report Results --

runner.report()

