Lua Engine
==================

SlideRule is a framework for developing high-performance distributed computing systems for analyzing science data in real-time.

The LuaEngine module encapsulates the Lua intepreter inside a C++ class.

The engine has three modes of operation: file, stream, and interactive.
**File mode** accepts a file or standard input and uses a ported version of the Lua command line client delivered with the Lua source in order to create an environment as close to possible to the delivered Lua interpreter.
**Stream mode** accepts Lua code blocks over a message queue and is used for remote Lua code execution.
**Interactive mode** provides a C++ class API to executing Lua code and allows other classes to leverage the Lua interpreter.
