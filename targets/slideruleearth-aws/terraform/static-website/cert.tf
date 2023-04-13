# Find a certificate issued by AWS
data "aws_acm_certificate" "domain" {
  domain      = "*.${var.domain}"
  statuses    = ["ISSUED"]
  types       = ["AMAZON_ISSUED"]
  most_recent = true
}