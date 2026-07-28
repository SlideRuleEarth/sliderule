"""MIME parsing and forward-message reconstruction.

This module is the heart of the gateway.  It uses the Python standard-library
:mod:`email` package (never naive string extraction) to:

* parse raw inbound MIME safely, tolerating malformed input;
* extract normalized metadata (sender, subject, reply-to, ...);
* rebuild a faithful forwarded copy that preserves text/HTML bodies,
  attachments, UTF-8 content and structure.

Because SES will only send mail whose ``From`` header belongs to a verified
identity, the forwarded message's ``From`` is rewritten to a verified address
while the *original* sender is preserved in the ``Reply-To`` and
``X-Original-Sender`` headers.  This is the standard, deliverable approach to
SES forwarding.
"""

from __future__ import annotations

import copy
from dataclasses import dataclass
from email import policy
from email.message import EmailMessage
from email.parser import BytesParser
from email.utils import formataddr, getaddresses, parseaddr
from typing import Any, Iterable

# Headers that must be stripped before re-sending: they either reference the
# original signing domain (now invalid because we rewrite ``From``) or are
# envelope/transport artifacts that SES will regenerate.
_HEADERS_TO_STRIP = (
    "DKIM-Signature",
    "Authentication-Results",
    "ARC-Seal",
    "ARC-Message-Signature",
    "ARC-Authentication-Results",
    "Return-Path",
    "Sender",
    "Received",
    "Received-SPF",
    "Resent-Sender",
)


class MailParseError(ValueError):
    """Raised when a MIME message cannot be parsed at all."""


# ---------------------------------------------------------------------------
# Address helpers
# ---------------------------------------------------------------------------
def normalize_address(raw: str | None) -> str:
    """Reduce an address (possibly with a display name) to a bare, lower-cased email.

    ``"John Smith <JP@SlideRuleEarth.io>"`` -> ``"jp@slideruleearth.io"``.
    Returns an empty string when no address can be extracted.
    """
    if not raw:
        return ""
    _display, addr = parseaddr(str(raw))
    return addr.strip().lower()


def normalize_addresses(raw: str | None) -> list[str]:
    """Normalize a header that may contain several addresses (To/Cc)."""
    if not raw:
        return []
    result: list[str] = []
    for _display, addr in getaddresses([str(raw)]):
        normalized = addr.strip().lower()
        if normalized:
            result.append(normalized)
    return result


def display_name_of(raw: str | None) -> str:
    """Extract just the display name portion of an address header."""
    if not raw:
        return ""
    display, _addr = parseaddr(str(raw))
    return display.strip()


# ---------------------------------------------------------------------------
# SES event parsing
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class SesNotification:
    """A single inbound-mail notification extracted from an SES Lambda event."""

    message_id: str
    source: str  # envelope sender (already provided by SES, may be empty)
    subject: str
    recipients: tuple[str, ...]
    spam_verdict: str
    virus_verdict: str
    spf_verdict: str
    dkim_verdict: str

    @property
    def is_spam(self) -> bool:
        """True if SES flagged the message as spam or containing a virus."""
        return self.spam_verdict == "FAIL" or self.virus_verdict == "FAIL"


def _verdict(receipt: dict[str, Any], key: str) -> str:
    node = receipt.get(key) or {}
    return str(node.get("status", "")).upper()


def extract_ses_notifications(event: dict[str, Any]) -> list[SesNotification]:
    """Parse an SES receipt-rule Lambda event into typed notifications.

    SES may batch multiple records per invocation; each is returned as one
    :class:`SesNotification`.  Records that are not SES mail are ignored.
    """
    notifications: list[SesNotification] = []
    for record in event.get("Records", []):
        ses = record.get("ses")
        if not ses:
            continue
        mail = ses.get("mail", {}) or {}
        receipt = ses.get("receipt", {}) or {}
        common = mail.get("commonHeaders", {}) or {}

        message_id = str(mail.get("messageId", "")).strip()
        if not message_id:
            # Without a messageId we cannot locate the archived object.
            continue

        notifications.append(
            SesNotification(
                message_id=message_id,
                source=normalize_address(mail.get("source")),
                subject=str(common.get("subject", "") or ""),
                recipients=tuple(receipt.get("recipients", []) or []),
                spam_verdict=_verdict(receipt, "spamVerdict"),
                virus_verdict=_verdict(receipt, "virusVerdict"),
                spf_verdict=_verdict(receipt, "spfVerdict"),
                dkim_verdict=_verdict(receipt, "dkimVerdict"),
            )
        )
    return notifications


# ---------------------------------------------------------------------------
# MIME parsing
# ---------------------------------------------------------------------------
@dataclass
class ParsedEmail:
    """A parsed inbound message plus convenient, normalized metadata.

    ``message`` is the live :class:`email.message.EmailMessage`; the scalar
    fields are cached extractions to keep call sites terse and testable.
    """

    message: EmailMessage
    from_address: str  # normalized bare address of the original sender
    from_header: str  # the raw original From header (with display name)
    reply_to: str  # original Reply-To header, or "" if absent
    subject: str
    to: str
    cc: str
    original_message_id: str

    @property
    def attachment_count(self) -> int:
        """Number of attachment parts (parts with a filename)."""
        return sum(1 for part in self.message.iter_attachments())


def parse_message(raw: bytes) -> ParsedEmail:
    """Parse raw MIME bytes into a :class:`ParsedEmail`.

    Uses ``policy.default`` (the modern :class:`EmailMessage` API) which is
    unicode-aware and tolerant of many real-world malformations.  Truly
    unparseable input raises :class:`MailParseError`.
    """
    try:
        message = BytesParser(policy=policy.default).parsebytes(raw)
    except Exception as exc:  # noqa: BLE001 - normalize any parser failure
        raise MailParseError(f"unable to parse MIME message: {exc}") from exc

    if not isinstance(message, EmailMessage):  # pragma: no cover - defensive
        raise MailParseError("parser did not return an EmailMessage")

    from_header = str(message.get("From", "") or "")
    return ParsedEmail(
        message=message,
        from_address=normalize_address(from_header),
        from_header=from_header,
        reply_to=str(message.get("Reply-To", "") or ""),
        subject=str(message.get("Subject", "") or ""),
        to=str(message.get("To", "") or ""),
        cc=str(message.get("Cc", "") or ""),
        original_message_id=str(message.get("Message-ID", "") or ""),
    )


# ---------------------------------------------------------------------------
# Forward reconstruction
# ---------------------------------------------------------------------------
def _delete_all(message: EmailMessage, header: str) -> None:
    """Remove *every* occurrence of *header* from *message*."""
    # ``del message[header]`` removes only the first occurrence, so loop.
    while header in message:
        del message[header]


def build_forward(
    parsed: ParsedEmail,
    *,
    forward_from: str,
    forwarded_by: str,
    to_header: str | None = None,
    extra_headers: Iterable[tuple[str, str]] | None = None,
) -> bytes:
    """Reconstruct a deliverable, faithful forward of *parsed*.

    Parameters
    ----------
    forward_from:
        A verified SES identity used as the new ``From`` address.
    forwarded_by:
        Value for the ``X-Forwarded-By`` header (usually the receiving
        gateway address, e.g. ``support@slideruleearth.io``).
    to_header:
        Optional override for the ``To`` header shown to recipients.  When
        omitted the original ``To`` header is preserved.  (Actual delivery
        recipients are controlled by the SES envelope, not this header.)
    extra_headers:
        Additional ``(name, value)`` headers to add (e.g. auto-generated
        tracking metadata).

    Returns
    -------
    bytes
        A fully serialized MIME message with CRLF line endings, ready for
        ``SendRawEmail`` / SES v2 ``send_email`` raw content.  All bodies and
        attachments from the original are preserved verbatim.
    """
    # Work on a deep copy so the caller's ParsedEmail stays pristine (handy
    # when broadcasting the same message to many recipients).
    forwarded: EmailMessage = copy.deepcopy(parsed.message)

    # Strip transport/signature headers that would be invalid post-rewrite.
    for header in _HEADERS_TO_STRIP:
        _delete_all(forwarded, header)

    # Preserve the original sender in a dedicated header, then rewrite From to
    # a verified identity so SES will accept the message.
    original_display = display_name_of(parsed.from_header) or parsed.from_address
    new_from_display = (
        f"{original_display} via SlideRule" if original_display else "SlideRule"
    )
    _delete_all(forwarded, "From")
    forwarded["From"] = formataddr((new_from_display, forward_from))

    # Reply-To: prefer the sender's original Reply-To, otherwise reply goes
    # straight back to the real author.
    reply_target = parsed.reply_to or parsed.from_header or parsed.from_address
    _delete_all(forwarded, "Reply-To")
    if reply_target:
        forwarded["Reply-To"] = reply_target

    # Optionally override the visible To header (used for list broadcasts).
    if to_header is not None:
        _delete_all(forwarded, "To")
        forwarded["To"] = to_header

    # Tracking / provenance headers.
    _delete_all(forwarded, "X-Original-Sender")
    _delete_all(forwarded, "X-Forwarded-By")
    if parsed.from_address:
        forwarded["X-Original-Sender"] = parsed.from_address
    forwarded["X-Forwarded-By"] = forwarded_by

    for name, value in extra_headers or ():
        _delete_all(forwarded, name)
        forwarded[name] = value

    # Serialize with the SMTP policy to guarantee correct CRLF line endings
    # and header folding for on-the-wire transmission.
    return forwarded.as_bytes(policy=policy.SMTP)


def build_auto_reply(
    *,
    to_address: str,
    from_address: str,
    subject: str,
    body: str,
    in_reply_to: str = "",
) -> bytes:
    """Build a simple UTF-8 auto-reply message.

    Used by the support handler to acknowledge receipt.  Returns serialized
    MIME bytes suitable for SES raw sending.
    """
    reply = EmailMessage()
    reply["From"] = from_address
    reply["To"] = to_address
    reply["Subject"] = subject
    # ``Auto-Submitted`` marks this as an automatic response so well-behaved
    # mailers do not bounce or auto-reply back (avoids mail loops).
    reply["Auto-Submitted"] = "auto-replied"
    reply["X-Forwarded-By"] = from_address
    if in_reply_to:
        reply["In-Reply-To"] = in_reply_to
        reply["References"] = in_reply_to
    reply.set_content(body, subtype="plain", charset="utf-8")
    return reply.as_bytes(policy=policy.SMTP)


def is_auto_submitted(parsed: ParsedEmail) -> bool:
    """Heuristic: True if the message looks automated (avoid reply loops).

    We skip auto-replies to messages that declare ``Auto-Submitted`` (other
    than ``no``), advertise bulk precedence, or come from a null/daemon sender.
    """
    auto = str(parsed.message.get("Auto-Submitted", "") or "").strip().lower()
    if auto and auto != "no":
        return True
    precedence = str(parsed.message.get("Precedence", "") or "").strip().lower()
    if precedence in {"bulk", "list", "junk", "auto_reply"}:
        return True
    if not parsed.from_address:
        return True
    local_part = parsed.from_address.split("@", 1)[0]
    if local_part in {"mailer-daemon", "postmaster", "no-reply", "noreply"}:
        return True
    return False
