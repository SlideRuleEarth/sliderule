# AWS Command Line

2021-04-22

## How To Install

To install:
```bash
$ sudo apt install awscli
```

## How To Get Credentials

When running from EC2, the credentials are automatically provided as a part of your IAM role.

When running from a local machine, there are a few steps you need to take before you can access your S3 bucket.

### Create an Access Key
1. Go to te **IAM** service on the AWS console (Integrated Access Management)
2. Navigate to **Access Management --> Users** (using the navigation bar on the left)
3. Select the **Security credentials** tab
4. Click on the **Create access key** button
    * This will give you an _Access Key_ and a _Secret Access Key_
    * You will need these keys in the future so make sure to keep the window up until these directions are completed

### Obtain your MFA ARN
1. While still in the **IAM** console, under the same ** Security credentials** tab, locate the **Sign-in credentials** section.
2. Copy down the **Assigned MFA device** id, it will look some like: `arn:aws:iam::12341234123:mfa/user`

### Configure your local machine
1. Configure your local credentials by going to a command prompt and running: `aws configure`
    * Copy and paste your _Access Key_ when prompted
    * Copy and paste your _Secret Access Key_ when prompted
    * Enter `us-west-2` for the region
    * Enter `json` for the output
2. Obtain a temporary token by running: `aws --profile=<Profile Name> sts get-session-token --serial-number <Assigned MFA device> --token-code <token>`
    * The **Assigned MFA device** is the id copied down above
    * The **token** is the MFA token (e.g. Google Authenticator) used to sign into the AWS console
3. The output of the command above will provide temporary credentials (for 12 hours):
```json
{
    "Credentials": {
        "SecretAccessKey": "secret-access-key",
        "SessionToken": "temporary-session-token",
        "Expiration": "expiration-date-time",
        "AccessKeyId": "access-key-id"
    }
}
```
4. Using the values provided, export those credentials to your local shell environemnt:
```bash
$ export AWS_ACCESS_KEY_ID=<access-key-id>
$ export AWS_SECRET_ACCESS_KEY=<secret-access-key>
$ export AWS_SESSION_TOKEN=<temporary-session-token>
```

Or, add them to the `[default]` section of your `.aws/credentials` file

5. Make sure that if you run the `get-session-token` command again that the exported variables above are ___not__ set.


## How To List S3 Files

```bash
$ aws s3 ls s3://icesat2-sliderule/<path_to_subdirectory>/
```

## How To Upload Files to S3

```bash
$ aws s3 cp <path_to_local_file> s3://icesat2-sliderule/<path_to_subdirectory>/
```

## How To List Multipart Uploads

```bash
$ aws s3api list-multipart-uploads --bucket icesat2-sliderule
```


