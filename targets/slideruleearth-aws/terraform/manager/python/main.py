#!/usr/bin/env python3

import os
import shutil
import boto3
import certbot.main

# Let’s Encrypt acme-v02 server that supports wildcard certificates
CERTBOT_SERVER = 'https://acme-v02.api.letsencrypt.org/directory'

# Temp dir of Lambda runtime
CERTBOT_DIR = '/tmp/certbot'


def rm_tmp_dir():
    if os.path.exists(CERTBOT_DIR):
        try:
            shutil.rmtree(CERTBOT_DIR)
        except NotADirectoryError:
            os.remove(CERTBOT_DIR)


def obtain_certs(emails, domains, dns_plugin):
    certbot_args = [
        # Override directory paths so script doesn't have to be run as root
        '--config-dir', CERTBOT_DIR,
        '--work-dir', CERTBOT_DIR,
        '--logs-dir', CERTBOT_DIR,

        # Obtain a cert but don't install it
        'certonly',

        # Run in non-interactive mode
        '--non-interactive',

        # Agree to the terms of service
        '--agree-tos',

        # Email of domain administrators
        '--email', emails,

        # Use dns challenge with dns plugin
        '--authenticator', dns_plugin,
        '--preferred-challenges', 'dns-01',

        # Use this server instead of default acme-v01
        '--server', CERTBOT_SERVER,

        # Domains to provision certs for (comma separated)
        '--domains', domains,
    ]
    return certbot.main.main(certbot_args)


# /tmp/certbot
# ├── live
# │   └── [domain]
# │       ├── README
# │       ├── cert.pem
# │       ├── chain.pem
# │       ├── fullchain.pem
# │       └── privkey.pem
def upload_certs(domains, s3_bucket, s3_prefix, s3_region):
    client = boto3.client('s3', s3_region)
    for domain in domains:
        cert_path = os.path.join(CERTBOT_DIR, 'live', domain)
        # needs to change: cat fullchain.pem privkey.pem | tee domain.pem
        local_path = os.path.join(cert_path, "cert.pem")
        remote_path = f'{s3_prefix}/{domain}.pem'
        print(f'Uploading: {local_path} => s3://{s3_bucket}/{s3_prefix}/{domain}.pem')
        client.upload_file(local_path, s3_bucket, remote_path)


def guarded_handler(event, context):
    # Input parameters from environment variables
    emails = os.getenv('EMAILS')
    domains = os.getenv('DOMAINS')
    dns_plugin = os.getenv('DNS_PLUGIN')
    s3_bucket = os.getenv('S3_BUCKET')  # The S3 bucket to publish certificates
    s3_prefix = os.getenv('S3_PREFIX')  # The S3 key subdirectory to publish certificates
    s3_region = os.getenv('S3_REGION')  # The AWS region of the S3 bucket

    obtain_certs(emails, domains, dns_plugin)
    upload_certs(domains, s3_bucket, s3_prefix, s3_region)

    return 'Certificates obtained and uploaded successfully.'


def lambda_handler(event, context):
    try:
        rm_tmp_dir()
        return guarded_handler(event, context)
    finally:
        rm_tmp_dir()