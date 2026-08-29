# =============================================================================
# Terraform Outputs
# =============================================================================

output "game_server_public_ip" {
  description = "Public IPv4 address of the Game Server instance"
  value       = aws_instance.game_server.public_ip
}

output "game_server_public_dns" {
  description = "Public DNS hostname of the Game Server instance"
  value       = aws_instance.game_server.public_dns
}

output "web_dashboard_url" {
  description = "URL for the Visual Arena Web Dashboard"
  value       = "http://${aws_instance.game_server.public_ip}:8080"
}

output "metrics_url" {
  description = "URL for the Prometheus Metrics Exporter endpoint"
  value       = "http://${aws_instance.game_server.public_ip}:9100/metrics"
}

output "grafana_dashboard_url" {
  description = "URL for Grafana Dashboards"
  value       = "http://${aws_instance.game_server.public_ip}:3000"
}
