local console_queue = "consoleq"

local console_monitor = core.monitor(core.LOG, console_queue)
console_monitor:name("console.monitor")

local console_file = core.file(core.WRITER, core.TEXT, "STDOUT")
console_file:name("console.file")

local console_writer = core.writer(console_file, console_queue)
console_writer:name("console.writer")

local console_dispatcher = core.dispatcher(core.MONITORQ)
console_dispatcher:name("console.dispatcher")
console_dispatcher:attach(console_monitor, "eventrec")
console_dispatcher:run()

local package = {
    q = console_queue,
    dispatcher = console_dispatcher,
    monitor = console_monitor,
    file = console_file,
    writer = console_writer
}

return package
