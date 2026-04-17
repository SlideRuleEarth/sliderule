-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
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
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    name = "Prometheus",
    description = "Provides application metrics in OpenMetrics Text Format used by Prometheus",
    logging = core.DEBUG,
    roles = {},
    signed = false,
    outputs = {"text"}
}
