resource "aws_s3_bucket" "docs_site_bucket" {
  bucket = var.domainName
  force_destroy = true
}
resource "aws_s3_bucket" "domain_apex_bucket" {
  bucket = var.domainApex
  force_destroy = true
}

resource "aws_s3_bucket_website_configuration" "redirect_apex_config" {
  bucket = aws_s3_bucket.domain_apex_bucket.bucket

  index_document {
    suffix = "index.html"
  }

  routing_rules = <<EOF
[
  {
    "Condition": {
      "KeyPrefixEquals": ""
    },
    "Redirect": {
      "HostName": "${var.domainName}",
      "Protocol": "https",
      "ReplaceKeyPrefixWith": "",
      "HttpRedirectCode": "301"
    }
  }
]
EOF
}

resource "aws_s3_bucket" "www_bucket" {
  bucket = "www.${var.domainApex}"
  force_destroy = true
}

resource "aws_s3_bucket_website_configuration" "redirect_www_config" {
  bucket = aws_s3_bucket.www_bucket.bucket

  index_document {
    suffix = "index.html"
  }

  routing_rules = <<EOF
[
  {
    "Condition": {
      "KeyPrefixEquals": ""
    },
    "Redirect": {
      "HostName": "${var.domainName}",
      "Protocol": "https",
      "ReplaceKeyPrefixWith": "",
      "HttpRedirectCode": "301"
    }
  }
]
EOF
}

resource "aws_s3_bucket_public_access_block" "example" {
  bucket = aws_s3_bucket.docs_site_bucket.id

  block_public_acls         = true
  block_public_policy       = true
  restrict_public_buckets   = true
  ignore_public_acls        = true
}

data "aws_iam_policy_document" "s3_policy" {
  statement {
    actions   = ["s3:GetObject"]
    resources = ["${aws_s3_bucket.docs_site_bucket.arn}/*"]

    principals {
      type        = "AWS"
      identifiers = [aws_cloudfront_origin_access_identity.origin_access_identity.iam_arn]
    }
  }
}

data "aws_caller_identity" "current" {
}


resource "aws_s3_bucket_policy" "web" {
  bucket = aws_s3_bucket.docs_site_bucket.id
  policy = data.aws_iam_policy_document.s3_policy.json
}