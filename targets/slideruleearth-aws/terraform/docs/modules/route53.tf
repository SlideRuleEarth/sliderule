data "aws_route53_zone" "public" {
  name         = var.domain_apex
  private_zone = false
}

resource "aws_acm_certificate" "mysite" {
  domain_name       = "*.${var.domain_apex}"
  validation_method = "DNS"
  lifecycle {
    create_before_destroy = true
  }
}

resource "aws_route53_record" "cert_validation" {
  allow_overwrite = true
  name            = tolist(aws_acm_certificate.mysite.domain_validation_options)[0].resource_record_name
  records         = [ tolist(aws_acm_certificate.mysite.domain_validation_options)[0].resource_record_value ]
  type            = tolist(aws_acm_certificate.mysite.domain_validation_options)[0].resource_record_type
  zone_id         = data.aws_route53_zone.public.id
  ttl             = 60
}


resource "aws_acm_certificate_validation" "cert" {
  certificate_arn         = aws_acm_certificate.mysite.arn
  validation_record_fqdns = [ aws_route53_record.cert_validation.fqdn ]
}

resource "aws_route53_record" "web" {
  zone_id = data.aws_route53_zone.public.id
  name    = var.domain_name

  type = "A"

  alias {
    name                   = aws_cloudfront_distribution.my_cloudfront.domain_name
    zone_id                = aws_cloudfront_distribution.my_cloudfront.hosted_zone_id
    evaluate_target_health = false
  }
}

resource "aws_route53_record" "redirect_apex" {
  zone_id = data.aws_route53_zone.public.id
  name    = "${var.domain_apex}"
  type    = "A"

  alias {
    name                   = aws_s3_bucket_website_configuration.redirect_apex_config.website_domain
    zone_id                = aws_s3_bucket.domain_apex_bucket.hosted_zone_id
    evaluate_target_health = false
  }
}

resource "aws_route53_record" "redirect_www" {
  zone_id = data.aws_route53_zone.public.id
  name    = "www.${var.domain_apex}"
  type    = "A"

  alias {
    name                   = aws_s3_bucket_website_configuration.redirect_www_config.website_domain
    zone_id                = aws_s3_bucket.www_bucket.hosted_zone_id
    evaluate_target_health = false
  }
}
