"""Lambda handler for ``users@slideruleearth.io``.

Flow
----
1. SES stores the raw MIME message in S3 and invokes this function.
2. The handler retrieves the archived message from S3 and parses it.
3. It verifies the sender is an authorized developer (exact, normalized,
   display-name-insensitive match).
   * If **not** authorized: log the attempt and return success so SES does
     not retry.  The message is **not** forwarded.
   * If authorized: retrieve every ``OPT_IN`` contact from the
     ``sliderule-users`` SES Contact List and broadcast the message to each.

Subscriber addresses are never hardcoded; they come from the Contact List.
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
class BroadcastResult:
    """Aggregate outcome of a broadcast to the subscriber list."""

    total: int
    succeeded: int
    failed: int


@dataclass(frozen=True)
class _Dependencies:
    """Immutable bundle of the handler's collaborators."""

    config: Config
    store: MessageStore
    ses: SesGateway
    logger: StructuredLogger


@lru_cache(maxsize=1)
def _dependencies() -> _Dependencies:
    """Build (and cache) the handler dependencies once per warm container."""
    config = Config.from_env()
    return _Dependencies(
        config=config,
        store=MessageStore(region=config.region),
        ses=SesGateway(region=config.region),
        logger=get_logger("users"),
    )


def handler(event: dict[str, Any], context: Any = None) -> dict[str, str]:
    """AWS Lambda entry point for inbound users-list mail."""
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
    """Process every SES notification in *event* (dependencies injected)."""
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
    """Authorize the sender and, if permitted, broadcast to subscribers."""
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

    # --- Retrieve + size-check ---------------------------------------------
    key = config.s3_key_for(message_id)
    raw = store.fetch_raw_message(config.project_bucket, key)
    if len(raw) > config.max_message_size:
        logger.warning(
            "rejected_oversize",
            message_id=message_id,
            sender=notification.source,
            size=len(raw),
            max_size=config.max_message_size,
        )
        return

    parsed = mail.parse_message(raw)

    # --- Authorization (exact, normalized, display-name-insensitive) -------
    if not config.is_authorized_sender(parsed.from_address):
        # Return success (do not raise) so SES/Lambda does not keep retrying
        # an intentionally-rejected message.
        logger.warning(
            "unauthorized_sender",
            message_id=message_id,
            sender=parsed.from_address,
            subject=parsed.subject,
            status="rejected",
        )
        return

    # --- Retrieve subscribers from the SES Contact List --------------------
    contacts = ses.list_opted_in_contacts(config.contact_list_name)
    recipients = [c.email for c in contacts]
    if not recipients:
        logger.info(
            "no_subscribers",
            message_id=message_id,
            sender=parsed.from_address,
            contact_list=config.contact_list_name,
        )
        return

    result = _broadcast(
        parsed,
        recipients=recipients,
        config=config,
        ses=ses,
        logger=logger,
        message_id=message_id,
    )

    logger.info(
        "broadcast_complete",
        message_id=message_id,
        sender=parsed.from_address,
        subject=parsed.subject,
        attachments=parsed.attachment_count,
        total=result.total,
        succeeded=result.succeeded,
        failed=result.failed,
        status="sent" if result.failed == 0 else "partial",
    )


def _broadcast(
    parsed: mail.ParsedEmail,
    *,
    recipients: list[str],
    config: Config,
    ses: SesGateway,
    logger: StructuredLogger,
    message_id: str,
) -> BroadcastResult:
    """Send the message individually to each subscriber.

    Sending one message per recipient keeps subscriber addresses private
    (no shared To/Cc) and lets SES attach per-recipient unsubscribe headers.
    Individual failures are logged but do not abort the whole broadcast.
    """
    # Present the list address in the visible To header rather than exposing
    # every subscriber; real delivery is controlled by the SES envelope.
    forwarded = mail.build_forward(
        parsed,
        forward_from=config.forward_from_address,
        forwarded_by=config.users_address,
        to_header=config.users_address,
    )

    succeeded = 0
    failed = 0
    for recipient in recipients:
        try:
            ses.send_raw(
                raw_message=forwarded,
                source=config.forward_from_address,
                destinations=[recipient],
                contact_list_name=config.contact_list_name,
                topic_name=config.contact_list_topic,
            )
            succeeded += 1
        except Exception:
            failed += 1
            logger.exception(
                "broadcast_send_failed",
                message_id=message_id,
                recipient=recipient,
            )
    return BroadcastResult(total=len(recipients), succeeded=succeeded, failed=failed)
