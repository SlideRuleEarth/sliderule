import os
import json
import boto3
from common import mail
from common import ses

# ###############################
# Globals
# ###############################

STACK_NAME = os.environ["STACK_NAME"]
PROJECT_BUCKET = os.environ.get("PROJECT_BUCKET")
SUPPORT_EMAIL = os.environ['SUPPORT_EMAIL']
SUPPORT_EMAILS = list({e.strip().lower() for e in os.environ['SUPPORT_EMAILS'].split(",")}) # recipients of support email

REJECT_SPAM = os.environ.get('REJECT_SPAM', True)
MAX_MESSAGE_SIZE = os.environ.get('MAX_MESSAGE_SIZE', 5 * 1048576) # defaults to 5MB
AUTO_REPLY = os.environ.get('AUTO_REPLY', True)


# ###############################
# Cached Objects
# ###############################

s3 = boto3.client("s3")


# ###############################
# Exceptions
# ###############################

class EmailProcessingError(Exception):
    def __init__(self, message, payload):
        super().__init__(message)
        self.payload = payload


# ###############################
# Utilities
# ###############################

def log(msg: str, parms: dict = None) -> None:
    if parms:
        print(json.dumps({"message": msg} | parms, default=str))
    else:
        print(json.dumps({"message": msg}))


def fetch_raw_message(self, bucket: str, key: str) -> bytes:
    """
    Return the raw MIME bytes stored at ``s3://{bucket}/{key}``.
    """
    try:
        response = self._client.get_object(Bucket=bucket, Key=key)
        body = response["Body"]
        data = body.read()
    except Exception as e:
        raise RuntimeError(f"unable to read archived message s3://{bucket}/{key}: {e}") from e
    return data


# ###############################
# Lambda Handler
# ###############################

def process_email(notification: mail.SesNotification) -> None:
    """
    Handle a single inbound message end to end.
    """
    # Spam / virus rejection
    if REJECT_SPAM and notification.is_spam:
        raise EmailProcessingError("rejected spam", {
            "message_id": notification.message_id,
            "sender": notification.source,
            "spam_verdict": notification.spam_verdict,
            "virus_verdict": notification.virus_verdict
        })

    # Retrieve archived message from S3
    raw = fetch_raw_message(PROJECT_BUCKET, f"{STACK_NAME}/{notification.message_id}")

    # Enforce maximum message size (configurable)
    if len(raw) > MAX_MESSAGE_SIZE:
        raise EmailProcessingError("rejected oversized message", {
            "message_id": notification.message_id,
            "sender": notification.source,
            "size": len(raw),
            "max_size": MAX_MESSAGE_SIZE
        })

    # Parse (tolerating malformed MIME)
    parsed = mail.parse_message(raw)

    # Present the original recipients in the forwarded "To" header so the
    # developers can see who the message was addressed to.
    forwarded = mail.build_forward(
        parsed,
        forward_from=SUPPORT_EMAIL,
        forwarded_by=SUPPORT_EMAIL,
        to_header=", ".join(SUPPORT_EMAILS),
    )

    # Forward to developers TODO: retry forward at least once
    try:
        sent_id = ses.send_raw(
            raw_message=forwarded,
            source=SUPPORT_EMAIL,
            destinations=SUPPORT_EMAILS,
        )
    except Exception as e:
        raise EmailProcessingError("forward failed", {
            "message_id": notification.message_id,
            "sender": parsed.from_address,
            "recipients": SUPPORT_EMAILS,
            "subject": parsed.subject,
            "attachments": parsed.attachment_count,
            "exception": f"{e}"
        })
    log("forwarded", {
        "message_id": notification.message_id,
        "ses_message_id": sent_id,
        "sender": parsed.from_address,
        "recipients": SUPPORT_EMAILS,
        "subject": parsed.subject,
        "attachments": parsed.attachment_count
    })

    # Send a best-effort acknowledgement
    if AUTO_REPLY and not mail.is_auto_submitted(parsed):
        reply = mail.build_auto_reply(
            to_address=parsed.from_address,
            from_address=SUPPORT_EMAIL,
            subject=f"Re: {parsed.subject}" if parsed.subject else "We received your message",
            body="Thank you for contacting SlideRule support.\nYour message has been received and forwarded to our team.",
            in_reply_to=parsed.original_message_id,
        )
        try:
            ses.send_raw(
                raw_message=reply,
                source=SUPPORT_EMAIL,
                destinations=[parsed.from_address],
            )
        except Exception as e:
            raise EmailProcessingError("auto reply failed", {
                "message_id": notification.message_id,
                "recipient": parsed.from_address,
                "exception": f"{e}"
            })
        log("auto reply sent", {
            "message_id": notification.message_id,
            "recipient": parsed.from_address
        })


#
# Support Lambda Handler (entrypoint)
#
def support_lambda_handler(event, context):
    """
    Lambda entry point
    """
    processed = 0 # see todo below
    succeeded = 0
    failed = 0
    for notification in mail.extract_ses_notifications(event):
        try:
            process_email(notification)
            succeeded += 1
        except EmailProcessingError as e:
            log(str(e), e.payload)
            failed += 1
    return {"status": "ok", "processed": "0"} # TODO: does this need to be a special format
