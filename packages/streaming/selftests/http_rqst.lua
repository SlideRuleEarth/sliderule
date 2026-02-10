local runner = require("test_executive")

-- Self Test --

runner.unittest("HTTP Request", function()
    local endpoint  = core.endpoint()
    local server    = core.httpd(10081):attach(endpoint, "/source"):untilup()
    local client    = streaming.http("127.0.0.1", 10081)
    local rsps, code, status = client:request("GET", "/source/health", "{}")
    runner.assert(status == true, "request failed")
    server:destroy()
    client:destroy()
end)

-- Report Results --

runner.report()

