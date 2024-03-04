resource "aws_cloudfront_response_headers_policy" "security_headers_policy" {
  name = "${var.domain_root}-docs-security-headers-policy"
  security_headers_config {
    content_type_options {
      override = true
    }
    frame_options {
      frame_option = "DENY"
      override     = true
    }
    referrer_policy {
      referrer_policy = "same-origin"
      override        = true
    }
    xss_protection {
      mode_block = true
      protection = true
      override   = true
    }
    strict_transport_security {
      access_control_max_age_sec = "63072000"
      include_subdomains         = true
      preload                    = true
      override                   = true
    }
    content_security_policy {
      content_security_policy = "frame-ancestors 'none'; default-src 'none'; img-src 'self' data:; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; style-src-elem 'self' https://cdn.jsdelivr.net 'unsafe-inline'; object-src 'none'; font-src 'self' https://cdn.jsdelivr.net ; connect-src 'self' https://*.testsliderule.org;"
      override                = true
    }
  }
}

resource "aws_cloudfront_function" "remove_web_from_uri" {
  name    = "remove-web-from-uri"
  runtime = "cloudfront-js-1.0"
  comment = "Function to remove /web from URI"

  # Use the file function to load the function code from a separate file
  # For example, if your JavaScript code is saved in 'remove-web-from-uri.js'
  code = <<-EOF
  function handler(event) {
      var request = event.request;
      var uri = request.uri;

      // Check if URI starts with /web and remove it
      if (uri.startsWith('/web')) {
          request.uri = uri.replace('/web', '');
      }
      // Check whether the URI is missing a file name.
      if (uri.endsWith('/')) {
          request.uri += 'index.html';
      } 
      // Check whether the URI is missing a file extension.
      else if (!uri.includes('.')) {
          request.uri += '/index.html';
      }

      return request;
  }
  EOF

}


resource "aws_cloudfront_distribution" "my_cloudfront" {
  depends_on = [
    aws_s3_bucket.docs_site_bucket
  ]

  origin {
    domain_name = aws_s3_bucket.docs_site_bucket.bucket_regional_domain_name
    origin_id   = "s3-cloudfront"

    s3_origin_config {
      origin_access_identity = aws_cloudfront_origin_access_identity.origin_access_identity.cloudfront_access_identity_path
    }
  }
  enabled             = true
  is_ipv6_enabled     = true
  default_root_object = "index.html"
  aliases = [var.domainName]
  
  restrictions {
    geo_restriction {
      restriction_type = "none"
    }
  }
  
  default_cache_behavior {
    allowed_methods  = ["DELETE", "GET", "HEAD", "OPTIONS", "PATCH", "POST", "PUT"]
    cached_methods   = ["GET", "HEAD"]
    target_origin_id = "s3-cloudfront"

    forwarded_values {
      query_string = false

      cookies {
        forward = "none"
      }
    }

    viewer_protocol_policy = "redirect-to-https"
    min_ttl                = 0
    default_ttl            = 3600
    max_ttl                = 86400

    response_headers_policy_id = aws_cloudfront_response_headers_policy.security_headers_policy.id

    function_association {
      event_type = "viewer-request"
      function_arn = aws_cloudfront_function.remove_web_from_uri.arn
    }

  }
  price_class = "PriceClass_200"

  viewer_certificate {
    cloudfront_default_certificate = true
    acm_certificate_arn = aws_acm_certificate.mysite.arn
    ssl_support_method       = "sni-only"
    minimum_protocol_version = "TLSv1"
  }

}

resource "aws_cloudfront_origin_access_identity" "origin_access_identity" {
  comment = "access-identity-${var.domainName}.s3.amazonaws.com"
}
