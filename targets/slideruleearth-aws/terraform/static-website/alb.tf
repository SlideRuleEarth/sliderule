# static-website Load Balancer
resource "aws_lb" "static-website" {
  name               = "${var.domain_root}-${var.Name_tag}-alb"
  load_balancer_type = "application"
  internal           = false
  security_groups    = [aws_security_group.alb-sg.id]
  subnets            = aws_subnet.public.*.id
  # access_logs {
  #  bucket = "sliderule"
  #  prefix = "access_logs/${domain}/static-website"
  #  enabled = true
  # }
  tags = {
    Name = "${var.domain_root}-${var.Name_tag}-alb"
  }
}

# Target group
resource "aws_alb_target_group" "static-website-target-group" {
  name     = "${var.domain_root}-${var.Name_tag}-alb-tg"
  port     = var.container_port
  protocol = "HTTP"
  vpc_id   = aws_vpc.static-website.id
  target_type = "ip"

  health_check {
    path                = var.static_web_health_check_path
    port                = "traffic-port"
    healthy_threshold   = 5
    unhealthy_threshold = 2
    timeout             = 2
    interval            = 5
    matcher             = "200"
  }
  tags = {
    Name = "${var.domain_root}-${var.Name_tag}-alb-tg"
  }
}

# Listener (redirects traffic from the load balancer to the target group)
resource "aws_alb_listener" "static-website-https-listener" {
  load_balancer_arn = aws_lb.static-website.id
  port              = 443
  protocol          = "HTTPS"
  ssl_policy        = "ELBSecurityPolicy-2016-08"
  certificate_arn   = data.aws_acm_certificate.domain.arn
  depends_on        = [aws_alb_target_group.static-website-target-group]

  default_action {
    type             = "forward"
    target_group_arn = aws_alb_target_group.static-website-target-group.arn
  }
  tags = {
    Name = "${var.domain_root}-${var.Name_tag}-https-lsnr"
  }
}
resource "aws_alb_listener" "static-website-http-listener" {
  load_balancer_arn = aws_lb.static-website.id
  port              = 80
  protocol          = "HTTP"
  default_action {
    type             = "redirect"
    redirect {
      port        = "443"
      protocol    = "HTTPS"
      status_code = "HTTP_301"
    }
  }
  tags = {
    Name = "${var.domain_root}-${var.Name_tag}-http-lsnr"
  }
}

# Route 53

data "aws_route53_zone" "selected" {
  name         = "${var.domain}"
}

resource "aws_route53_record" "static-website" {
  zone_id = data.aws_route53_zone.selected.zone_id
  name    = "www.${data.aws_route53_zone.selected.name}"
  type    = "A"
  allow_overwrite = true
  alias {
    name                   = aws_lb.static-website.dns_name
    zone_id                = aws_lb.static-website.zone_id
    evaluate_target_health = false
  }
}
resource "aws_route53_record" "static-website-base" {
  zone_id = data.aws_route53_zone.selected.zone_id
  name    = "${data.aws_route53_zone.selected.name}"
  type    = "A"
  allow_overwrite = true
  alias {
    name                   = aws_lb.static-website.dns_name
    zone_id                = aws_lb.static-website.zone_id
    evaluate_target_health = false
  }
}