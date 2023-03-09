# HAProxy and Let's Encrpyt

2023-01-18

These instructions detail the steps neccessary to create the **pem** file needed by HAProxy to perform SSL termination - which means clients can access our web services via **https** instead of **http**.  We will first request a certificate from Let's Encrypt for our domain, and then we will format that certificate into a format recognizable by HAProxy.  Lastly, we will configure HAProxy to use the certificate to terminate SSL (i.e. support https on the frontend and pass along http to the backend).

## Setup workstation with proper credentials

To run Certbot for a deployment in AWS, we need a workstation that has the credentials needed to verify control over our domain.  This can be accomplished using the policy json file pasted below.  This policy must be attached to the role associated with the workstation you are running on.

{
    "Version": "2012-10-17",
    "Id": "certbot-dns-route53 sample policy",
    "Statement": [
        {
            "Effect": "Allow",
            "Action": [
                "route53:ListHostedZones",
                "route53:GetChange"
            ],
            "Resource": [
                "*"
            ]
        },
        {
            "Effect" : "Allow",
            "Action" : [
                "route53:ChangeResourceRecordSets"
            ],
            "Resource" : [
                "arn:aws:route53:::hostedzone/YOURHOSTEDZONEID"
            ]
        }
    ]
}

Note - YOURHOSTEDZONEID is the ID associated with slideruleearth.io hosted zone

## Installing Certbot on Ubuntu

At the time of this writing, detailed instructions for installing and using Certbot can be found at: https://certbot.eff.org/instructions?ws=haproxy&os=ubuntufocal.  Select the "Wildcard" tab for instructions on how to use Certbot when a certificate is needed for a machine other than the one you are running from.
```bash
sudo snap install core; sudo snap refresh core
sudo snap install --classic certbot
sudo ln -s /snap/bin/certbot /usr/bin/certbot
sudo snap set certbot trust-plugin-with-root=ok
sudo snap install certbot-dns-route53
```

## Run Certbot to generate a certificate

The first time you run Certbot, it will prompt you with a few questions that need to be answered.  After that, it will run to completion without prompts.

To run Certbot and generate the certificate files needed for HAProxy, run the following command:
```bash
sudo certbot certonly --dns-route53 -d *.slideruleearth.io
```

This will produce output that looks like:
```
Saving debug log to /var/log/letsencrypt/letsencrypt.log
Requesting a certificate for *.slideruleearth.io

Successfully received certificate.
Certificate is saved at: /etc/letsencrypt/live/slideruleearth.io/fullchain.pem
Key is saved at:         /etc/letsencrypt/live/slideruleearth.io/privkey.pem
This certificate expires on YYY-MM-DD.
```

## Concat the files provided by Certbot into a file readable by HAProxy

HAProxy needs a single pem file for its SSL configuration.  That pem file can be created with the following:
```bash
sudo cat /etc/letsencrypt/live/slideruleearth.io/fullchain.pem /etc/letsencrypt/live/slideruleearth.io/privkey.pem | sudo tee slideruleearth.io.pem
```

The file should then be copied/moved to a location accessible to HAProxy:
```bash
sudo cp slideruleearth.io.pem /etc/ssl/private
```