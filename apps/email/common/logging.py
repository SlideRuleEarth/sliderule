"""Structured (JSON) logging for the SlideRule mail gateway.

CloudWatch Logs indexes JSON payloads, which makes structured logs far easier
to search and build metric filters on than free-form text.  Every log record
emitted through :class:`StructuredLogger` is a single JSON object with a
stable ``event`` field plus arbitrary structured context (sender, recipients,
message id, forwarding status, errors, ...).

The module deliberately keeps no mutable module-level state beyond the stdlib
logger registry so it is safe to import from anywhere.
"""

from __future__ import annotations

import json
import logging
import os
from dataclasses import dataclass
from typing import Any

# Standard logging level names accepted via the ``LOG_LEVEL`` env var.
_DEFAULT_LEVEL = os.environ.get("LOG_LEVEL", "INFO").upper()


class _JsonFormatter(logging.Formatter):
    """Format records that already carry a JSON string message.

    :class:`StructuredLogger` serializes structured payloads to JSON before
    handing them to the stdlib logger, so the formatter simply passes the
    message through while adding the level and (when present) exception text.
    """

    def format(self, record: logging.LogRecord) -> str:
        message = record.getMessage()
        # If an exception is attached, fold its text into the JSON payload so
        # the stack trace stays on the same CloudWatch log record.
        if record.exc_info:
            try:
                payload = json.loads(message)
            except (ValueError, TypeError):
                payload = {"event": "log", "message": message}
            payload["exception"] = self.formatException(record.exc_info)
            payload.setdefault("level", record.levelname)
            return json.dumps(payload, default=str)
        return message


def _configure_root() -> None:
    """Attach a single JSON handler to the root logger (idempotent).

    In the AWS Lambda runtime a handler is pre-installed on the root logger.
    We reconfigure it once to emit JSON rather than the default text format.
    """
    root = logging.getLogger()
    root.setLevel(_DEFAULT_LEVEL)
    formatter = _JsonFormatter()
    if root.handlers:
        for handler in root.handlers:
            handler.setFormatter(formatter)
    else:
        handler = logging.StreamHandler()
        handler.setFormatter(formatter)
        root.addHandler(handler)


@dataclass(frozen=True)
class StructuredLogger:
    """A thin wrapper that emits structured JSON log records.

    Example
    -------
    >>> log = get_logger(__name__)
    >>> log.info("forwarded", message_id="abc", recipients=["a@b.com"])
    """

    _logger: logging.Logger

    def _emit(self, level: int, event: str, exc_info: bool = False, **fields: Any) -> None:
        payload: dict[str, Any] = {"event": event, "level": logging.getLevelName(level)}
        # Merge structured context; ``default=str`` guarantees serialization
        # of unexpected types (e.g. datetimes) never raises.
        for key, value in fields.items():
            payload[key] = value
        self._logger.log(level, json.dumps(payload, default=str), exc_info=exc_info)

    def debug(self, event: str, **fields: Any) -> None:
        self._emit(logging.DEBUG, event, **fields)

    def info(self, event: str, **fields: Any) -> None:
        self._emit(logging.INFO, event, **fields)

    def warning(self, event: str, **fields: Any) -> None:
        self._emit(logging.WARNING, event, **fields)

    def error(self, event: str, *, exc_info: bool = False, **fields: Any) -> None:
        self._emit(logging.ERROR, event, exc_info=exc_info, **fields)

    def exception(self, event: str, **fields: Any) -> None:
        """Log an error together with the current exception's stack trace."""
        self._emit(logging.ERROR, event, exc_info=True, **fields)


def get_logger(name: str) -> StructuredLogger:
    """Return a :class:`StructuredLogger` bound to *name*."""
    _configure_root()
    return StructuredLogger(logging.getLogger(name))
