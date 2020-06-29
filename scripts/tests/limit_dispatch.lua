local runner = require("test_executive")
local console = require("console")

console.logger:config(core.INFO)

-- Limit Dispatch Unit Test Setup --

runner.command("DEFINE test.rec id 8")
runner.command("ADD_FIELD test.rec id INT32 0 1 NATIVE")
runner.command("ADD_FIELD test.rec counter INT32 4 1 NATIVE")

idlimit = core.limit("id", nil, 50, 150, nil, "limit_limitq")
idlimit:name("idlimit")

counterlimit = core.limit("counter", 50, 11, 10000, nil, "limit_limitq")
counterlimit:name("counterlimit")

r = core.dispatcher("limit_inputq")
r:name("dispatcher")
r:attach(idlimit, "test.rec")
r:attach(counterlimit, "test.rec")

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
        runner.check('filter_id == 1.0',          string.format('FILTER_ID is incorrect: %d', filter_id))
        runner.check('limit_min == 1.0',          string.format('LIMIT_MIN is incorrect: %d', limit_min))
        runner.check('limit_max == 1.0',          string.format('LIMIT_MAX is incorrect: %d', limit_max))
        runner.check('id        == 50.0',         string.format('ID is incorrect: %f', id))
        runner.check('d_min     == 11.0',         string.format('D_MIN is incorrect: %f', d_min))
        runner.check('d_max     == 10000.0',      string.format('D_MAX is incorrect: %f', d_max))
        runner.check('field_name == "counter"',   string.format('FIELD_NAME is incorrect: %s', field_name))
        runner.check('record_name == "test.rec"', string.format('RECORD_NAME is incorrect: %s', record_name))
        valsum = valsum + d_val
    else
        runner.check(false, "Timeout receiving limit")
        break
    end
end

-- Compare Results --

runner.check(valsum == 100110, string.format('Failed to correctly report all limit violation values %d', valsum))

-- Report Results --

runner.report()

