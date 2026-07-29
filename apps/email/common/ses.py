"""Thin, testable wrappers around the SES / SES v2 APIs.

Two capabilities are needed by the gateway:

1. **Sending** a fully-formed raw MIME message (SES v2 ``send_email`` with
   ``Raw`` content).  This preserves attachments and structure exactly.
2. **Reading contacts** from an SES *Contact List*, filtered to subscribers
   whose status is ``OPT_IN`` (SES v2 ``list_contacts``).

The class accepts an injected boto3 client so unit tests can supply a stub
without touching AWS, and it holds no mutable state of its own.
"""

from dataclasses import dataclass
from typing import Any, Sequence

import boto3


@dataclass(frozen=True)
class Contact:
    """A single subscribed contact retrieved from an SES Contact List."""

    email: str
    opted_in: bool


class SesGateway:
    """Wrapper over the SES v2 client for sending and contact retrieval."""

    def __init__(self, *, client: Any | None = None, region: str | None = None) -> None:
        # ``sesv2`` supports both raw sending and the Contact List APIs.
        self._client = client or boto3.client("sesv2", region_name=region or None)

    # ------------------------------------------------------------------
    # Sending
    # ------------------------------------------------------------------
    def send_raw(
        self,
        *,
        raw_message: bytes,
        source: str,
        destinations: Sequence[str],
        contact_list_name: str = "",
        topic_name: str = "",
    ) -> str:
        """Send a raw MIME message and return the SES message id.

        Parameters
        ----------
        raw_message:
            The serialized MIME bytes (see :func:`common.mail.build_forward`).
        source:
            Envelope ``From`` -- must be a verified SES identity.
        destinations:
            Envelope recipients (the true delivery targets).
        contact_list_name / topic_name:
            When both are supplied, SES adds standards-compliant
            ``List-Unsubscribe`` headers via its list-management feature.
        """
        request: dict[str, Any] = {
            "FromEmailAddress": source,
            "Destination": {"ToAddresses": list(destinations)},
            "Content": {"Raw": {"Data": raw_message}},
        }
        if contact_list_name and topic_name:
            request["ListManagementOptions"] = {
                "ContactListName": contact_list_name,
                "TopicName": topic_name,
            }
        response = self._client.send_email(**request)
        return str(response.get("MessageId", ""))

    # ------------------------------------------------------------------
    # Contacts
    # ------------------------------------------------------------------
    def list_opted_in_contacts(self, contact_list_name: str) -> list[Contact]:
        """Return every ``OPT_IN`` contact from *contact_list_name*.

        Handles pagination transparently.  Only contacts whose subscription
        status resolves to ``OPT_IN`` are returned.
        """
        contacts: list[Contact] = []
        next_token: str | None = None
        while True:
            kwargs: dict[str, Any] = {
                "ContactListName": contact_list_name,
                # Server-side filter to opted-in subscribers only.
                "Filter": {"FilteredStatus": "OPT_IN"},
                "PageSize": 100,
            }
            if next_token:
                kwargs["NextToken"] = next_token
            response = self._client.list_contacts(**kwargs)
            for entry in response.get("Contacts", []):
                email = str(entry.get("EmailAddress", "")).strip().lower()
                if not email:
                    continue
                contacts.append(Contact(email=email, opted_in=True))
            next_token = response.get("NextToken")
            if not next_token:
                break
        return contacts
