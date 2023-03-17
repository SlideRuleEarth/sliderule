output "static-website-alb-address" {
  value = aws_lb.static-website.dns_name
}
