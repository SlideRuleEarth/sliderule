local console_monitor = core.logmon(core.INFO, core.FMT_TEXT):global("console.monitor")

local function loglvl (lvl)
    console_monitor:config(lvl)
    sys.setcfg("log_level", lvl)
end

local package = {
    monitor = console_monitor,
    loglvl = loglvl
}

return package
