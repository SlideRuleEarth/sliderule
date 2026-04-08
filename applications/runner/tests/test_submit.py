from runner import submit_handler
import pytest
import base64

lua_script = base64.b64encode("""
print("Hello World")
return "Nice to meet you", true
""".encode()).decode()

def test_missing_name():
    with pytest.raises(KeyError):
        rsps = submit_handler({}, "user")

def test_invalid_name():
    with pytest.raises(KeyError):
        rsps = submit_handler({"name": 1}, "user")

def test_empty_script():
    with pytest.raises(KeyError):
        rsps = submit_handler({"name": "hello_world", "script":""}, "user")

def test_invalid_args_list():
    with pytest.raises(RuntimeError):
        rsps = submit_handler({"name": "hello_world", "script":lua_script, "args_list": "test1"}, "user")

def test_invalid_vcpus():
    with pytest.raises(RuntimeError):
        rsps = submit_handler({"name": "hello_world", "script":lua_script, "args_list": [""], "vcpus": 50}, "user")

def test_invalid_memory():
    with pytest.raises(RuntimeError):
        rsps = submit_handler({"name": "hello_world", "script":lua_script, "args_list": [""], "memory": "lots"}, "user")
