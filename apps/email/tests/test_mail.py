"""Unit tests for common.mail (parsing, normalization, forward rebuild)."""

from __future__ import annotations

from email import policy
from email.parser import BytesParser

import mail
from conftest import make_raw_email, make_ses_event


def test_normalize_address_strips_display_name():
    assert mail.normalize_address("John Smith <JP@SlideRuleEarth.io>") == "jp@slideruleearth.io"
    assert mail.normalize_address("plain@example.com") == "plain@example.com"
    assert mail.normalize_address(None) == ""
    assert mail.normalize_address("") == ""


def test_normalize_addresses_multiple():
    header = "A <a@x.com>, B <b@y.com>"
    assert mail.normalize_addresses(header) == ["a@x.com", "b@y.com"]


def test_parse_message_extracts_metadata():
    raw = make_raw_email(subject="Test", reply_to="reply@example.com")
    parsed = mail.parse_message(raw)
    assert parsed.from_address == "alice@example.com"
    assert parsed.subject == "Test"
    assert parsed.reply_to == "reply@example.com"
    assert parsed.attachment_count == 1


def test_parse_message_malformed_still_parses():
    # Missing headers / garbage should not crash the tolerant parser.
    parsed = mail.parse_message(b"not really an email")
    assert parsed.from_address == ""


def test_build_forward_preserves_bodies_and_attachment():
    raw = make_raw_email()
    parsed = mail.parse_message(raw)
    forwarded = mail.build_forward(
        parsed,
        forward_from="support@slideruleearth.io",
        forwarded_by="support@slideruleearth.io",
        to_header="dev@slideruleearth.io",
    )
    rebuilt = BytesParser(policy=policy.default).parsebytes(forwarded)

    # From rewritten to verified identity; original sender preserved.
    assert mail.normalize_address(rebuilt["From"]) == "support@slideruleearth.io"
    assert rebuilt["X-Original-Sender"] == "alice@example.com"
    assert rebuilt["X-Forwarded-By"] == "support@slideruleearth.io"
    assert mail.normalize_address(rebuilt["Reply-To"]) == "alice@example.com"

    # Text + HTML + attachment survive the round trip.
    assert rebuilt.get_body(preferencelist=("plain",)) is not None
    assert rebuilt.get_body(preferencelist=("html",)) is not None
    assert sum(1 for _ in rebuilt.iter_attachments()) == 1


def test_build_forward_prefers_existing_reply_to():
    raw = make_raw_email(reply_to="custom@example.com")
    parsed = mail.parse_message(raw)
    forwarded = mail.build_forward(
        parsed,
        forward_from="support@slideruleearth.io",
        forwarded_by="support@slideruleearth.io",
    )
    rebuilt = BytesParser(policy=policy.default).parsebytes(forwarded)
    assert mail.normalize_address(rebuilt["Reply-To"]) == "custom@example.com"


def test_extract_ses_notifications():
    event = make_ses_event(message_id="abc", spam="FAIL")
    notes = mail.extract_ses_notifications(event)
    assert len(notes) == 1
    assert notes[0].message_id == "abc"
    assert notes[0].is_spam is True


def test_is_auto_submitted_detects_loops():
    raw = make_raw_email(sender="mailer-daemon@example.com")
    parsed = mail.parse_message(raw)
    assert mail.is_auto_submitted(parsed) is True
