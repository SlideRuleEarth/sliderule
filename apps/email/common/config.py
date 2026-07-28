"""Typed, immutable configuration for the SlideRule mail gateway.

All runtime configuration is supplied to the Lambda functions through
environment variables (populated by CloudFormation).  Nothing is hardcoded.

The :class:`Config` dataclass is *frozen* so that a loaded configuration
object cannot be accidentally mutated at runtime, which keeps the handlers
free of global mutable state and makes them trivial to unit test -- tests can
build a :class:`Config` directly instead of monkeypatching the environment.
"""

from __future__ import annotations

import os
from dataclasses import dataclass, field
from typing import Mapping

# ---------------------------------------------------------------------------
# Environment variable names (single source of truth).
#
# Keeping the names in one place avoids typos and documents the full contract
# between the CloudFormation template and the Lambda code.
# ---------------------------------------------------------------------------
ENV_DOMAIN = "DOMAIN"
ENV_DEVELOPER_EMAILS = "DEVELOPER_EMAILS"
ENV_SUPPORT_ADDRESS = "SUPPORT_ADDRESS"
ENV_USERS_ADDRESS = "USERS_ADDRESS"
ENV_PROJECT_BUCKET = "PROJECT_BUCKET"
ENV_CONTACT_LIST_NAME = "CONTACT_LIST_NAME"
ENV_CONTACT_LIST_TOPIC = "CONTACT_LIST_TOPIC"
ENV_S3_PREFIX = "S3_PREFIX"
ENV_FORWARD_FROM_ADDRESS = "FORWARD_FROM_ADDRESS"
ENV_REJECT_SPAM = "REJECT_SPAM"
ENV_MAX_MESSAGE_SIZE = "MAX_MESSAGE_SIZE"
ENV_AUTO_REPLY_ENABLED = "AUTO_REPLY_ENABLED"
ENV_AUTO_REPLY_SUBJECT = "AUTO_REPLY_SUBJECT"
ENV_AUTO_REPLY_BODY = "AUTO_REPLY_BODY"
ENV_REGION = "AWS_REGION"  # provided automatically by the Lambda runtime

# Default maximum accepted message size: 10 MB.  SES itself caps inbound mail
# at 40 MB, but forwarding very large messages is often undesirable.
DEFAULT_MAX_MESSAGE_SIZE = 10 * 1024 * 1024


class ConfigError(RuntimeError):
    """Raised when required configuration is missing or invalid."""


def _require(env: Mapping[str, str], name: str) -> str:
    """Return a required environment variable or raise :class:`ConfigError`."""
    value = env.get(name, "").strip()
    if not value:
        raise ConfigError(f"missing required environment variable: {name}")
    return value


def _optional(env: Mapping[str, str], name: str, default: str = "") -> str:
    """Return an optional environment variable, falling back to *default*."""
    value = env.get(name)
    if value is None:
        return default
    value = value.strip()
    return value if value else default


def _parse_bool(raw: str, *, default: bool = False) -> bool:
    """Parse a permissive boolean string ("true"/"1"/"yes"/"on")."""
    if raw == "":
        return default
    return raw.strip().lower() in {"1", "true", "yes", "on", "enabled"}


def _parse_int(raw: str, *, default: int) -> int:
    """Parse an integer, falling back to *default* on empty/invalid input."""
    try:
        return int(raw)
    except (TypeError, ValueError):
        return default


def parse_email_list(raw: str) -> tuple[str, ...]:
    """Split a comma-separated list of email addresses.

    Addresses are lower-cased and stripped; empty entries and duplicates are
    removed while preserving the original order.  Display names, if any, are
    left intact here -- normalization to a bare address is the responsibility
    of :func:`common.mail.normalize_address`.
    """
    seen: set[str] = set()
    result: list[str] = []
    for part in raw.split(","):
        candidate = part.strip()
        if not candidate:
            continue
        key = candidate.lower()
        if key in seen:
            continue
        seen.add(key)
        result.append(candidate)
    return tuple(result)


@dataclass(frozen=True)
class Config:
    """Immutable runtime configuration for a mail-gateway Lambda."""

    # --- Required ----------------------------------------------------------
    domain: str
    developer_emails: tuple[str, ...]
    support_address: str
    users_address: str
    project_bucket: str

    # --- Derived / optional with sensible defaults -------------------------
    contact_list_name: str = "sliderule-users"
    contact_list_topic: str = ""
    s3_prefix: str = ""
    forward_from_address: str = ""
    region: str = ""

    # --- Nice-to-have feature flags ---------------------------------------
    reject_spam: bool = False
    max_message_size: int = DEFAULT_MAX_MESSAGE_SIZE
    auto_reply_enabled: bool = False
    auto_reply_subject: str = "We received your message"
    auto_reply_body: str = (
        "Thank you for contacting SlideRule support. "
        "Your message has been received and forwarded to our team."
    )

    # Pre-computed, normalized set of authorized senders (bare addresses).
    developer_addresses: frozenset[str] = field(default_factory=frozenset)

    @classmethod
    def from_env(cls, env: Mapping[str, str] | None = None) -> "Config":
        """Build a :class:`Config` from environment variables.

        Parameters
        ----------
        env:
            Mapping to read from.  Defaults to :data:`os.environ`.  Passing an
            explicit mapping keeps this method pure and unit-testable.
        """
        # Import here to avoid a circular import (mail imports nothing from
        # config at module load, but keeping this local is defensive).
        from common.mail import normalize_address

        source: Mapping[str, str] = os.environ if env is None else env

        developer_emails = parse_email_list(_require(source, ENV_DEVELOPER_EMAILS))
        support_address = _require(source, ENV_SUPPORT_ADDRESS)
        users_address = _require(source, ENV_USERS_ADDRESS)

        # The address used as the SMTP "From" when re-sending.  It must be a
        # verified SES identity within the domain; default to the support
        # address which is always verified as part of the domain.
        forward_from = _optional(source, ENV_FORWARD_FROM_ADDRESS) or support_address

        developer_addresses = frozenset(
            normalize_address(addr) for addr in developer_emails
        )

        return cls(
            domain=_require(source, ENV_DOMAIN),
            developer_emails=developer_emails,
            support_address=support_address,
            users_address=users_address,
            project_bucket=_require(source, ENV_PROJECT_BUCKET),
            contact_list_name=_optional(
                source, ENV_CONTACT_LIST_NAME, "sliderule-users"
            ),
            contact_list_topic=_optional(source, ENV_CONTACT_LIST_TOPIC),
            s3_prefix=_optional(source, ENV_S3_PREFIX),
            forward_from_address=forward_from,
            region=_optional(source, ENV_REGION),
            reject_spam=_parse_bool(_optional(source, ENV_REJECT_SPAM)),
            max_message_size=_parse_int(
                _optional(source, ENV_MAX_MESSAGE_SIZE),
                default=DEFAULT_MAX_MESSAGE_SIZE,
            ),
            auto_reply_enabled=_parse_bool(_optional(source, ENV_AUTO_REPLY_ENABLED)),
            auto_reply_subject=_optional(
                source, ENV_AUTO_REPLY_SUBJECT, "We received your message"
            ),
            auto_reply_body=_optional(
                source,
                ENV_AUTO_REPLY_BODY,
                "Thank you for contacting SlideRule support. "
                "Your message has been received and forwarded to our team.",
            ),
            developer_addresses=developer_addresses,
        )

    def s3_key_for(self, message_id: str) -> str:
        """Return the S3 object key for a given SES ``messageId``.

        SES stores received mail at ``{ObjectKeyPrefix}{messageId}``; the
        prefix is configured per-address through :data:`ENV_S3_PREFIX`.
        """
        prefix = self.s3_prefix
        if prefix and not prefix.endswith("/"):
            prefix = prefix + "/"
        return f"{prefix}{message_id}"

    def is_authorized_sender(self, address: str) -> bool:
        """Return ``True`` if *address* (bare, normalized) may broadcast."""
        return address in self.developer_addresses
