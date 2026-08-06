
import sys
from email.message import EmailMessage
from email import policy
from pathlib import Path
import pytest

# Make the app package importable when running ``pytest`` from this directory.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from handler import Contact  # noqa: E402

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
