"""Lambda handler for ``support@slideruleearth.io``.

Flow
----
1. SES stores the raw MIME message in S3 and invokes this function.
2. The handler retrieves the archived message from S3.
3. It parses the MIME (preserving attachments, text/HTML bodies, subject,
   sender and CC) and reconstructs a faithful forward.
4. It forwards the message to the configured developer list.
5. Optionally sends an auto-reply to the original sender.
6. On failure it logs a structured error to CloudWatch; the original email
   remains archived in S3 regardless.

The handler wires up module-level clients (reused across warm invocations)
but delegates all logic to :func:`process_event`, which takes its
dependencies as parameters and is therefore straightforward to unit test.
"""

from __future__ import annotations

from dataclasses import dataclass
from functools import lru_cache
from typing import Any

from common import mail
from common.config import Config
from common.logging import StructuredLogger, get_logger
from common.mail import SesNotification
from common.ses import SesGateway
from common.storage import MessageStore


@dataclass(frozen=True)
class _Dependencies:
    """Immutable bundle of the handler's collaborators."""

    config: Config
    store: MessageStore
    ses: SesGateway
    logger: StructuredLogger


@lru_cache(maxsize=1)
def _dependencies() -> _Dependencies:
    """Build (and cache) the handler dependencies once per warm container.

    Initialization is lazy so that importing this module never requires the
    environment to be configured -- which keeps unit tests import-safe and
    surfaces configuration errors as clear handler failures.
    """
    config = Config.from_env()
    return _Dependencies(
        config=config,
        store=MessageStore(region=config.region),
        ses=SesGateway(region=config.region),
        logger=get_logger("support"),
    )


def handler(event: dict[str, Any], context: Any = None) -> dict[str, str]:
    """AWS Lambda entry point for inbound support mail."""
    deps = _dependencies()
    return process_event(
        event,
        config=deps.config,
        store=deps.store,
        ses=deps.ses,
        logger=deps.logger,
    )


def process_event(
    event: dict[str, Any],
    *,
    config: Config,
    store: MessageStore,
    ses: SesGateway,
    logger: StructuredLogger,
) -> dict[str, str]:
    """Process every SES notification in *event*.

    Dependencies are injected so tests can pass stubs.  A structured summary
    dict is returned (useful for tests and for the Lambda console).
    """
    processed = 0
    for notification in mail.extract_ses_notifications(event):
        _process_one(notification, config=config, store=store, ses=ses, logger=logger)
        processed += 1
    return {"status": "ok", "processed": str(processed)}


def _process_one(
    notification: SesNotification,
    *,
    config: Config,
    store: MessageStore,
    ses: SesGateway,
    logger: StructuredLogger,
) -> None:
    """Handle a single inbound message end to end."""
    message_id = notification.message_id

    # --- Spam / virus rejection (configurable) -----------------------------
    if config.reject_spam and notification.is_spam:
        logger.warning(
            "rejected_spam",
            message_id=message_id,
            sender=notification.source,
            spam_verdict=notification.spam_verdict,
            virus_verdict=notification.virus_verdict,
        )
        return

    # --- Retrieve archived message from S3 ---------------------------------
    key = config.s3_key_for(message_id)
    raw = store.fetch_raw_message(config.project_bucket, key)

    # --- Enforce maximum message size (configurable) -----------------------
    if len(raw) > config.max_message_size:
        logger.warning(
            "rejected_oversize",
            message_id=message_id,
            sender=notification.source,
            size=len(raw),
            max_size=config.max_message_size,
        )
        return

    # --- Parse (tolerating malformed MIME) ---------------------------------
    parsed = mail.parse_message(raw)

    # Present the original recipients in the forwarded "To" header so the
    # developers can see who the message was addressed to.
    developers = list(config.developer_emails)
    forwarded = mail.build_forward(
        parsed,
        forward_from=config.forward_from_address,
        forwarded_by=config.support_address,
        to_header=", ".join(developers),
    )

    # --- Forward to developers --------------------------------------------
    try:
        sent_id = ses.send_raw(
            raw_message=forwarded,
            source=config.forward_from_address,
            destinations=developers,
        )
    except Exception:
        # Log a useful, structured error.  Re-raise so the async Lambda
        # retry can recover from transient SES issues; the original mail is
        # still safely archived in S3.
        logger.exception(
            "forward_failed",
            message_id=message_id,
            sender=parsed.from_address,
            recipients=developers,
            subject=parsed.subject,
            attachments=parsed.attachment_count,
        )
        raise

    logger.info(
        "forwarded",
        message_id=message_id,
        ses_message_id=sent_id,
        sender=parsed.from_address,
        recipients=developers,
        subject=parsed.subject,
        attachments=parsed.attachment_count,
        status="sent",
    )

    # --- Optional auto-reply to the original sender ------------------------
    if config.auto_reply_enabled and not mail.is_auto_submitted(parsed):
        _send_auto_reply(parsed, config=config, ses=ses, logger=logger, message_id=message_id)


def _send_auto_reply(
    parsed: mail.ParsedEmail,
    *,
    config: Config,
    ses: SesGateway,
    logger: StructuredLogger,
    message_id: str,
) -> None:
    """Send a best-effort acknowledgement; never fail the main flow on error."""
    try:
        reply = mail.build_auto_reply(
            to_address=parsed.from_address,
            from_address=config.support_address,
            subject=f"Re: {parsed.subject}" if parsed.subject else config.auto_reply_subject,
            body=config.auto_reply_body,
            in_reply_to=parsed.original_message_id,
        )
        ses.send_raw(
            raw_message=reply,
            source=config.support_address,
            destinations=[parsed.from_address],
        )
        logger.info(
            "auto_reply_sent",
            message_id=message_id,
            recipient=parsed.from_address,
        )
    except Exception:
        logger.exception(
            "auto_reply_failed",
            message_id=message_id,
            recipient=parsed.from_address,
        )
