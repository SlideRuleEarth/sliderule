from provisioner import send_email
import os

def test_email(email):
    os.environ["ALERT_EMAIL"] = email
    assert send_email(f"SlideRule Provisioner Test", "Hello World")