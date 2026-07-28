"""Retrieval of archived MIME messages from S3.

SES stores each received message as an S3 object at
``{ObjectKeyPrefix}{messageId}``.  The gateway Lambdas fetch those raw bytes
back for parsing and forwarding.  This module isolates the S3 access so the
handlers can be unit tested with a stub client.
"""

from __future__ import annotations

from typing import Any

import boto3


class MessageNotFoundError(RuntimeError):
    """Raised when the archived message cannot be retrieved from S3."""


class MessageStore:
    """Read-only accessor for archived MIME messages in S3."""

    def __init__(self, *, client: Any | None = None, region: str | None = None) -> None:
        self._client = client or boto3.client("s3", region_name=region or None)

    def fetch_raw_message(self, bucket: str, key: str) -> bytes:
        """Return the raw MIME bytes stored at ``s3://{bucket}/{key}``.

        Raises :class:`MessageNotFoundError` if the object is missing or
        unreadable so callers can log a clear diagnostic.
        """
        try:
            response = self._client.get_object(Bucket=bucket, Key=key)
            body = response["Body"]
            data = body.read()
        except Exception as exc:  # noqa: BLE001 - normalize S3/client errors
            raise MessageNotFoundError(
                f"unable to read archived message s3://{bucket}/{key}: {exc}"
            ) from exc
        return data
