from runner import submit_handler
import base64

lua_script = base64.b64encode("""
print("Hello World")
return "Nice to meet you", true
""".encode()).decode()

def test_missing_name():
    rsps = submit_handler({}, "user")
    assert rsps['statusCode'] == 500

def test_invalid_name():
    rsps = submit_handler({"name": 1}, "user")
    assert rsps['statusCode'] == 500

def test_empty_script():
    rsps = submit_handler({"name": "hello_world", "script":""}, "user")
    assert rsps['statusCode'] == 500

def test_invalid_args_list():
    rsps = submit_handler({"name": "hello_world", "script":lua_script, "args_list": "test1"}, "user")
    assert rsps['statusCode'] == 500

def test_invalid_vcpus():
    rsps = submit_handler({"name": "hello_world", "script":lua_script, "args_list": [""], "vcpus": 50}, "user")
    assert rsps['statusCode'] == 500

def test_invalid_memory():
    rsps = submit_handler({"name": "hello_world", "script":lua_script, "args_list": [""], "memory": "lots"}, "user")
    assert rsps['statusCode'] == 500
