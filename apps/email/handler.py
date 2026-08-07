import os
import json
import boto3
import mail
from dataclasses import dataclass
from typing import Any, Sequence


# ###############################
# Globals
# ###############################

USERS_EMAIL         = mail.normalize_address(os.environ['USERS_EMAIL'])
SUPPORT_EMAIL       = mail.normalize_address(os.environ['SUPPORT_EMAIL'])
SUPPORT_EMAILS      = list({e.strip().lower() for e in os.environ['SUPPORT_EMAILS'].split(",")}) # recipients of support email
PROJECT_BUCKET      = os.environ['PROJECT_BUCKET']
CONTACT_LIST_NAME   = os.environ['CONTACT_LIST_NAME']
CONTACT_LIST_TOPIC  = os.environ['CONTACT_LIST_TOPIC']
S3_PREFIX           = os.environ["S3_PREFIX"]

REJECT_SPAM         = True
MAX_MESSAGE_SIZE    = 5 * 1048576 # 5MB
AUTO_REPLY          = True


# ###############################
# Cached Objects
# ###############################

s3 = boto3.client("s3")
ses = boto3.client("sesv2")


# ###############################
# Classes
# ###############################

class EmailProcessingError(Exception):
    """
    Exception used for all email processing errors.
    """
    def __init__(self, message, payload):
        super().__init__(message)
        self.payload = payload


@dataclass(frozen=True)
class Contact:
    """
    A single subscribed contact retrieved from an SES Contact List.
    """
    email: str
    opted_in: bool


# ###############################
# Utilities
# ###############################

#
# Log a message to CloudWatch
#
def log(msg: str, parms: dict = None) -> None:
    if parms:
        print(json.dumps({"message": msg} | parms, default=str))
    else:
        print(json.dumps({"message": msg}))

#
# Fetch a raw email from S3
#
def fetch_raw_email(bucket: str, key: str) -> bytes:
    """
    Return the raw MIME bytes stored at ``s3://{bucket}/{key}``.
    """
    try:
        response = s3.get_object(Bucket=bucket, Key=key)
        body = response["Body"]
        data = body.read()
    except Exception as e:
        raise RuntimeError(f"unable to read archived message s3://{bucket}/{key}: {e}") from e
    return data

#
# Send a raw email
#
def send_raw_email(*, raw_message: bytes, source: str, destinations: Sequence[str], contact_list_name: str = "", topic_name: str = "") -> str:
    """
    Send a raw MIME message and return the SES message id.

    Parameters
    ----------
    raw_message:
        The serialized MIME bytes (see :func:`mail.build_forward`).
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
    response = ses.send_email(**request)
    return str(response.get("MessageId", ""))

#
# Get list of contacts opted in to a topic
#
def list_opted_in_contacts(contact_list_name: str) -> list[Contact]:
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
        response = ses.list_contacts(**kwargs)
        for entry in response.get("Contacts", []):
            email = str(entry.get("EmailAddress", "")).strip().lower()
            if not email:
                continue
            contacts.append(Contact(email=email, opted_in=True))
        next_token = response.get("NextToken")
        if not next_token:
            break
    return contacts


# ###############################
# Handle Lambdas
# ###############################

#
# Support Email Processing
#
def support_email_processor(parsed: mail.ParsedEmail, notification_message_id: str) -> None:
    """
    Handle a single inbound message end to end.
    """
    # build forwarded email to send to developers
    forwarded = mail.build_forward(
        parsed,
        forward_from=SUPPORT_EMAIL,
        forwarded_by=SUPPORT_EMAIL,
        to_header=", ".join(SUPPORT_EMAILS),
    )

    # forward email to developers
    try:
        sent_id = send_raw_email(
            raw_message=forwarded,
            source=SUPPORT_EMAIL,
            destinations=SUPPORT_EMAILS,
        )
        log("forwarded", {
            "message_id": notification_message_id,
            "ses_message_id": sent_id,
            "sender": parsed.from_address,
            "recipients": SUPPORT_EMAILS,
            "subject": parsed.subject,
            "attachments": parsed.attachment_count
        })
    except Exception as e:
        raise EmailProcessingError("forward failed", {
            "message_id": notification_message_id,
            "sender": parsed.from_address,
            "recipients": SUPPORT_EMAILS,
            "subject": parsed.subject,
            "attachments": parsed.attachment_count,
            "exception": f"{e}"
        })

    # send a best-effort acknowledgement
    if AUTO_REPLY and not mail.is_auto_submitted(parsed):
        reply = mail.build_auto_reply(
            to_address=parsed.from_address,
            from_address=SUPPORT_EMAIL,
            subject=f"Re: {parsed.subject}" if parsed.subject else "We received your message",
            body="Thank you for contacting SlideRule support.\nYour message has been received and forwarded to our team.",
            in_reply_to=parsed.original_message_id,
        )
        try:
            send_raw_email(
                raw_message=reply,
                source=SUPPORT_EMAIL,
                destinations=[parsed.from_address],
            )
            log("auto reply sent", {
                "message_id": notification_message_id,
                "recipient": parsed.from_address
            })
        except Exception as e:
            raise EmailProcessingError("auto reply failed", {
                "message_id": notification_message_id,
                "recipient": parsed.from_address,
                "exception": f"{e}"
            })

#
#  Users Email Processing
#
def users_email_processor(parsed: mail.ParsedEmail, notification_message_id: str) -> None:
    """
    Authorize the sender and, if permitted, broadcast to subscribers.
    """
    # authorization check (exact, normalized, display-name-insensitive)
    if parsed.from_address not in SUPPORT_EMAILS:
        raise EmailProcessingError("unauthorized sender", {
            "message_id": notification_message_id,
            "sender": parsed.from_address,
            "subject": parsed.subject
        })

    # present the list address in the visible To header rather than exposing
    # every subscriber; real delivery is controlled by the SES envelope.
    forwarded = mail.build_forward(
        parsed,
        forward_from=SUPPORT_EMAIL,
        forwarded_by=SUPPORT_EMAIL,
        to_header=USERS_EMAIL,
    )

    # send email to each subscriber from the SES Contact List that has opted in
    recipients = [contact.email for contact in list_opted_in_contacts(CONTACT_LIST_NAME)]
    for recipient in recipients:
        try:
            send_raw_email(
                raw_message=forwarded,
                source=SUPPORT_EMAIL,
                destinations=[recipient],
                contact_list_name=CONTACT_LIST_NAME,
                topic_name=CONTACT_LIST_TOPIC,
            )
        except Exception as e:
            log("broadcast send failed", {
                "message_id": notification_message_id,
                "recipient": recipient,
                "exception": str(e)
            })

    # complete
    log("broadcast complete", {
        "message_id": notification_message_id,
        "sender": parsed.from_address,
        "subject": parsed.subject,
        "attachments": parsed.attachment_count,
        "recipients": len(recipients),
        "contact_list_name": CONTACT_LIST_NAME,
        "contact_list_topic": CONTACT_LIST_TOPIC
    })

#
# Process SES Notifications
#
def process_notifications(event, processor):
    """
    1. Check for spam
    2. Fetch raw email from S3
    3. Parse raw email into class object
    4. Process specific type of email
    5. Handle errors
    """
    for notification in mail.extract_ses_notifications(event):
        try:
            # spam rejection
            if REJECT_SPAM and notification.is_spam:
                raise EmailProcessingError("rejected spam", {
                    "message_id": notification.message_id,
                    "sender": notification.source,
                    "spam_verdict": notification.spam_verdict,
                    "virus_verdict": notification.virus_verdict
                })

            # retrieve archived message from S3
            raw = fetch_raw_email(PROJECT_BUCKET, f"{S3_PREFIX}{notification.message_id}")

            # enforce maximum message size
            if len(raw) > MAX_MESSAGE_SIZE:
                raise EmailProcessingError("rejected oversized message", {
                    "message_id": notification.message_id,
                    "sender": notification.source,
                    "size": len(raw),
                    "max_size": MAX_MESSAGE_SIZE
                })

            # parse email (tolerating malformed MIME)
            parsed = mail.parse_message(raw)

            # process email with passed-in email processor
            processor(parsed, notification.message_id)

        # handle errors
        except EmailProcessingError as e:
            log(str(e), e.payload)

#
# Support Lambda
#
def support_lambda(event, context):
    """
    Lambda entry point for support emails
    """
    process_notifications(event, support_email_processor)

#
# Users Lambda
#
def users_lambda(event, context):
    """
    Lambda entry point for users emails
    """
    process_notifications(event, users_email_processor)
