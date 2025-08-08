local runner = require("test_executive")

-- Requirements --

if not __legacy__ then
	return runner.skip()
end

-- Limit Dispatch Unit Test Setup --

runner.command("DEFINE test.rec id 8")
runner.command("ADD_FIELD test.rec id INT32 0 1 NATIVE")
runner.command("ADD_FIELD test.rec counter INT32 4 1 NATIVE")

local idlimit = streaming.limit("id", nil, 50, 150, nil, "limit_limitq")

local counterlimit = streaming.limit("counter", 50, 11, 10000, nil, "limit_limitq")

local r = streaming.dispatcher("limit_inputq")
r:attach(idlimit, "test.rec"):attach(counterlimit, "test.rec"):run()

local inputq = msg.publish("limit_inputq")
local limitq = msg.subscribe("limit_limitq")

-- Send Test Records --

for i=1,10010,1 do
    local testrec1 = msg.create(string.format('test.rec id=50 counter=%d', i))
    inputq:sendrecord(testrec1)
    local testrec2 = msg.create(string.format('test.rec id=150 counter=%d', i))
    inputq:sendrecord(testrec2)
end

-- Receive Limitss --

valsum = 0.0
for i=1,20,1 do
    local limit = limitq:recvrecord(1000)
    if limit then
        filter_id     = limit:getvalue("FILTER_ID")
        limit_min     = limit:getvalue("LIMIT_MIN")
        limit_max     = limit:getvalue("LIMIT_MAX")
        id            = limit:getvalue("ID")
        d_min         = limit:getvalue("D_MIN")
        d_max         = limit:getvalue("D_MAX")
        d_val         = limit:getvalue("D_VAL")
        field_name    = limit:getvalue("FIELD_NAME")
        record_name   = limit:getvalue("RECORD_NAME")
        runner.assert('filter_id == 1.0',          string.format('FILTER_ID is incorrect: %d', filter_id))
        runner.assert('limit_min == 1.0',          string.format('LIMIT_MIN is incorrect: %d', limit_min))
        runner.assert('limit_max == 1.0',          string.format('LIMIT_MAX is incorrect: %d', limit_max))
        runner.assert('id        == 50.0',         string.format('ID is incorrect: %f', id))
        runner.assert('d_min     == 11.0',         string.format('D_MIN is incorrect: %f', d_min))
        runner.assert('d_max     == 10000.0',      string.format('D_MAX is incorrect: %f', d_max))
        runner.assert('field_name == "counter"',   string.format('FIELD_NAME is incorrect: %s', field_name))
        runner.assert('record_name == "test.rec"', string.format('RECORD_NAME is incorrect: %s', record_name))
        valsum = valsum + d_val
    else
        runner.assert(false, "Timeout receiving limit")
        break
    end
end

-- Compare Results --

runner.assert(valsum == 100110, string.format('Failed to correctly report all limit violation values %d', valsum))

-- Clean Up --

r:destroy()
idlimit:destroy()
counterlimit:destroy()

-- Report Results --

runner.report()

