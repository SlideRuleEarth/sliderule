import os
import importlib
import provisioner

def test_email(email):
    os.environ["ALERT_EMAIL"] = email
    importlib.reload(provisioner)
    assert provisioner.send_email(f"SlideRule Provisioner Test", "Hello World")