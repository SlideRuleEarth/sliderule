--
-- ENDPOINT:    /source/prometheus
--
-- INPUT:       N/A
--
-- OUTPUT:      OpenMetrics Text Format (used by prometheus)
--

local rsp_str = ""

local metrics = sys.metric()

for k, v in pairs(metrics) do
    local metric_name = k:gsub("[%.-:]","_")
    local metric_type = v["type"]
    local metric_value = tostring(v["value"])
    rsp_str = rsp_str .. string.format("\n# TYPE %s %s\n", metric_name, metric_type)
    rsp_str = rsp_str .. string.format("%s %s\n", metric_name, metric_value)
end

return rsp_str
