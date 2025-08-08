data "aws_route53_zone" "selected" {
  name = "${var.domain}"
}

resource "aws_route53_record" "org" {
  zone_id         = data.aws_route53_zone.selected.zone_id
  name            = "${var.cluster_name}.${data.aws_route53_zone.selected.name}"
  type            = "A"
  ttl             = 300
  allow_overwrite = true
  records         = [aws_instance.ilb.public_ip]
}