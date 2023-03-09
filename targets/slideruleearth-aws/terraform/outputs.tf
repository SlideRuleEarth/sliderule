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
