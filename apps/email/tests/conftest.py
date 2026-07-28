"""Shared pytest fixtures and lightweight test doubles.

The stubs here let the handler logic be exercised without any AWS access,
demonstrating the unit-testable, dependency-injected design.
"""

from __future__ import annotations

import sys
from email.message import EmailMessage
from email import policy
from pathlib import Path

import pytest

# Make the app package importable when running ``pytest`` from this directory.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from common.config import Config  # noqa: E402
from common.ses import Contact  # noqa: E402


@pytest.fixture
def config() -> Config:
    """A representative, fully-populated Config built from a fake environment."""
    env = {
        "DOMAIN": "slideruleearth.io",
        "DEVELOPER_EMAILS": "Dev One <dev1@slideruleearth.io>, dev2@slideruleearth.io",
        "SUPPORT_ADDRESS": "support@slideruleearth.io",
        "USERS_ADDRESS": "users@slideruleearth.io",
        "PROJECT_BUCKET": "sliderule-project",
        "CONTACT_LIST_NAME": "sliderule-users",
        "CONTACT_LIST_TOPIC": "broadcast",
        "S3_PREFIX": "email/support/",
        "FORWARD_FROM_ADDRESS": "support@slideruleearth.io",
        "AWS_REGION": "us-east-1",
        "AUTO_REPLY_ENABLED": "true",
    }
    return Config.from_env(env)


def make_raw_email(
    *,
    sender: str = "Alice Example <alice@example.com>",
    to: str = "support@slideruleearth.io",
    subject: str = "Héllo wörld",
    text: str = "Plain body ✓",
    html: str | None = "<p>HTML body ✓</p>",
    attachment: bytes | None = b"attachment-bytes",
    reply_to: str | None = None,
) -> bytes:
    """Construct a realistic multipart MIME message for tests."""
    msg = EmailMessage()
    msg["From"] = sender
    msg["To"] = to
    msg["Subject"] = subject
    msg["Message-ID"] = "<orig@example.com>"
    if reply_to:
        msg["Reply-To"] = reply_to
    msg.set_content(text, charset="utf-8")
    if html is not None:
        msg.add_alternative(html, subtype="html", charset="utf-8")
    if attachment is not None:
        msg.add_attachment(
            attachment,
            maintype="application",
            subtype="octet-stream",
            filename="data.bin",
        )
    return msg.as_bytes(policy=policy.SMTP)


def make_ses_event(message_id: str = "msg-123", **overrides) -> dict:
    """Build a minimal SES receipt-rule Lambda event."""
    return {
        "Records": [
            {
                "eventSource": "aws:ses",
                "ses": {
                    "mail": {
                        "messageId": message_id,
                        "source": overrides.get("source", "alice@example.com"),
                        "commonHeaders": {"subject": overrides.get("subject", "Hi")},
                    },
                    "receipt": {
                        "recipients": ["support@slideruleearth.io"],
                        "spamVerdict": {"status": overrides.get("spam", "PASS")},
                        "virusVerdict": {"status": overrides.get("virus", "PASS")},
                        "spfVerdict": {"status": "PASS"},
                        "dkimVerdict": {"status": "PASS"},
                    },
                },
            }
        ]
    }


class FakeStore:
    """Stub MessageStore returning a preset payload."""

    def __init__(self, raw: bytes) -> None:
        self.raw = raw
        self.requests: list[tuple[str, str]] = []

    def fetch_raw_message(self, bucket: str, key: str) -> bytes:
        self.requests.append((bucket, key))
        return self.raw


class FakeSes:
    """Stub SesGateway recording sends and returning canned contacts."""

    def __init__(self, contacts: list[Contact] | None = None, fail: bool = False) -> None:
        self.contacts = contacts or []
        self.fail = fail
        self.sends: list[dict] = []

    def send_raw(self, *, raw_message, source, destinations, **kwargs) -> str:
        if self.fail:
            raise RuntimeError("simulated SES failure")
        self.sends.append(
            {
                "raw": raw_message,
                "source": source,
                "destinations": list(destinations),
                **kwargs,
            }
        )
        return f"ses-{len(self.sends)}"

    def list_opted_in_contacts(self, contact_list_name: str) -> list[Contact]:
        return list(self.contacts)
