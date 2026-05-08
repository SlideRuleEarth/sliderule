-------------------------------------------------------
-- initialization
-------------------------------------------------------
local global    = require("global")
local json      = require("json")
local parms     = json.decode(arg[1])

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()

    -- get parameters
    local type = global.eval(parms["type"]) or core.LOG
    local duration = parms["duration"] or 600

    -- greate dispatcher and publisher
    local dispatcher = streaming.dispatcher(core.EVENTQ)
    local publisher = streaming.publish(_rqst.rspq)

    -- attach publisher to specified record types
    if type & core.LOG          then dispatcher:attach(publisher, "eventrec")       end
    if type & core.TRACE        then dispatcher:attach(publisher, "tracerec")       end
    if type & core.TELEMETRY    then dispatcher:attach(publisher, "telemetryrec")   end
    if type & core.ALERT        then dispatcher:attach(publisher, "exceptrec")      end

    -- run dispatcher
    dispatcher:run()

    -- watch response queue
    local watchq = msg.publish(_rqst.rspq)

    -- pend for duration (in 1 second intervals to allow hooks to execute) --
    local seconds = 0
    if duration > 600 then duration = 600 end
    while (watchq:numsubs() > 0) and (seconds < duration) do
        seconds = seconds + 1
        sys.wait(1)
    end

end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "Events",
    description = "List system events that occur for given duration",
    logging = core.CRITICAL,
    roles = {"member", "owner"},
    signed = false,
    inputs = {"json"},
    outputs = {"binary"},
    schema = {
        request = [[ "application/json": {
            "schema": {
                "type": "object",
                "properties": {
                    "type": {
                        "type": "integer",
                        "description": "Bitmask of event types to subscribe to (core.LOG=1, core.TRACE=2, core.TELEMETRY=4, core.ALERT=8)",
                        "default": 1
                    },
                    "duration": {
                        "type": "integer",
                        "description": "Seconds to hold connection open; max 600",
                        "default": 600
                    }
                }
            }
        } ]],
        response = [[ "application/octet-stream": {
            "schema": {
                "allOf": [
                    { "$ref": "../components/schemas/eventrec.json" }
                ],
                "description": "Stream of binary-encoded event records"
            }
        } ]]
    }
}