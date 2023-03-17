# demo Load Balancer
# demo Load Balancer
resource "aws_lb" "demo" {
  name               = "${var.domain_root}-${var.Name_tag}-alb"
  load_balancer_type = "application"
  internal           = false
  security_groups    = [aws_security_group.alb-sg.id]
  subnets            = aws_subnet.public.*.id
  # access_logs {
  #  bucket = "sliderule"
  #  prefix = "access_logs/${domain}/demo"
  #  enabled = true
  # }
  tags = {
    Name = "${var.domain_root}-${var.Name_tag}-alb"
  }
}

# Target group
resource "aws_alb_target_group" "demo-target-group" {
  name     = "${var.domain_root}-${var.Name_tag}-alb-tg"
  port     = var.nginx_container_port
  protocol = "HTTP"
  vpc_id   = aws_vpc.demo.id
  target_type = "ip"

  health_check {
    path                = var.demo_health_check_path
    port                = "traffic-port"
    healthy_threshold   = 5
    unhealthy_threshold = 5
    timeout             = 2
    interval            = 5
    matcher             = "200"
  }
  stickiness {
    enabled = true
    type = "lb_cookie"
  }
  tags = {
    Name = "${var.domain_root}-${var.Name_tag}-alb-tg"
  }
}
data "aws_acm_certificate" "sliderule_cluster_cert" {
  domain      = "*.${var.domain}"
  types       = ["AMAZON_ISSUED"]
  most_recent = true
}
# Listener (redirects traffic from the load balancer to the target group)
resource "aws_alb_listener" "demo-https-listener" {
  load_balancer_arn = aws_lb.demo.id
  port              = 443
  protocol          = "HTTPS"
  ssl_policy        = "ELBSecurityPolicy-2016-08"
  certificate_arn   = data.aws_acm_certificate.sliderule_cluster_cert.arn
  depends_on        = [aws_alb_target_group.demo-target-group]

  default_action {
    type             = "forward"
    target_group_arn = aws_alb_target_group.demo-target-group.arn
  }
  tags = {
    Name = "${var.domain_root}-${var.Name_tag}-https-lsnr"
  }
}

resource "aws_alb_listener" "demo-http-listener" {
  load_balancer_arn = aws_lb.demo.id
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

resource "aws_route53_record" "demo-site" {
  zone_id = data.aws_route53_zone.selected.zone_id
  name    = "demo.${data.aws_route53_zone.selected.name}"
  type    = "A"
  allow_overwrite = true
  alias {
    name                   = aws_lb.demo.dns_name
    zone_id                = aws_lb.demo.zone_id
    evaluate_target_health = false
  }
}