"""Unit tests for the support and users Lambda handlers (fully stubbed)."""

from __future__ import annotations

import support_lambda
import users_lambda
from common.logging import get_logger
from common.ses import Contact
from conftest import FakeSes, FakeStore, make_raw_email, make_ses_event

LOG = get_logger("test")


# --------------------------------------------------------------------------
# support handler
# --------------------------------------------------------------------------
def test_support_forwards_to_developers(config):
    store = FakeStore(make_raw_email())
    ses = FakeSes()
    result = support_lambda.process_event(
        make_ses_event(), config=config, store=store, ses=ses, logger=LOG
    )
    assert result["status"] == "ok"
    # One forward to developers (+ one auto-reply since enabled).
    forwards = [s for s in ses.sends if s["destinations"] != ["alice@example.com"]]
    assert forwards and forwards[0]["destinations"] == list(config.developer_emails)


def test_support_reraises_on_send_failure(config):
    store = FakeStore(make_raw_email())
    ses = FakeSes(fail=True)
    try:
        support_lambda.process_event(
            make_ses_event(), config=config, store=store, ses=ses, logger=LOG
        )
    except RuntimeError:
        pass
    else:  # pragma: no cover
        raise AssertionError("expected forward failure to propagate")


# --------------------------------------------------------------------------
# users handler
# --------------------------------------------------------------------------
def _users_config(config):
    # Reuse the support config but point the S3 prefix at users mail.
    from dataclasses import replace

    return replace(config, s3_prefix="email/users/", auto_reply_enabled=False)


def test_users_rejects_unauthorized_sender(config):
    cfg = _users_config(config)
    store = FakeStore(make_raw_email(sender="stranger@example.com"))
    ses = FakeSes(contacts=[Contact("sub@example.com", True)])
    result = users_lambda.process_event(
        make_ses_event(), config=cfg, store=store, ses=ses, logger=LOG
    )
    # Returns success but sends nothing.
    assert result["status"] == "ok"
    assert ses.sends == []


def test_users_broadcasts_for_authorized_sender(config):
    cfg = _users_config(config)
    store = FakeStore(make_raw_email(sender="dev1@slideruleearth.io"))
    ses = FakeSes(
        contacts=[Contact("a@example.com", True), Contact("b@example.com", True)]
    )
    users_lambda.process_event(
        make_ses_event(), config=cfg, store=store, ses=ses, logger=LOG
    )
    sent_to = sorted(d for s in ses.sends for d in s["destinations"])
    assert sent_to == ["a@example.com", "b@example.com"]


def test_users_partial_failure_does_not_abort(config):
    cfg = _users_config(config)
    store = FakeStore(make_raw_email(sender="dev1@slideruleearth.io"))

    class FlakySes(FakeSes):
        def send_raw(self, *, raw_message, source, destinations, **kwargs):
            if destinations == ["b@example.com"]:
                raise RuntimeError("boom")
            return super().send_raw(
                raw_message=raw_message, source=source, destinations=destinations, **kwargs
            )

    ses = FlakySes(
        contacts=[Contact("a@example.com", True), Contact("b@example.com", True)]
    )
    result = users_lambda.process_event(
        make_ses_event(), config=cfg, store=store, ses=ses, logger=LOG
    )
    assert result["status"] == "ok"
    # a@ still received the message despite b@ failing.
    assert any(s["destinations"] == ["a@example.com"] for s in ses.sends)
