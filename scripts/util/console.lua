local console_queue = "consoleq"

local console_logger = core.logger(console_queue, core.CRITICAL)
console_logger:name("console.logger")

local console_file = core.file(core.WRITER, core.TEXT, "STDOUT")
console_file:name("console.file")

local console_writer = core.writer(console_file, console_queue)
console_writer:name("console.writer")

local package = {
    q = console_queue,
    logger = console_logger,
    file = console_file,
    writer = console_writer
}

return package
