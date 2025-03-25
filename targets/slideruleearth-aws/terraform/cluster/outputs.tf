output "ilb_ip_address" {
    value = aws_instance.ilb.public_ip
    description = "The public ipv4 address of the load balancer"
}
output "ilb_state" {
    value = aws_instance.ilb.instance_state
}
output "ilb_id" {
    value = aws_instance.ilb.id
}
output "monitor_state" {
    value = aws_instance.monitor.instance_state
}
output "monitor_id" {
    value = aws_instance.monitor.id
}
output "spot_max_price_out" {
    value = var.spot_max_price
}
output "spot_allocation_stragtegy_out" {
    value = var.spot_allocation_strategy
}
output "availability_zone_out" {
    value = var.availability_zone
}