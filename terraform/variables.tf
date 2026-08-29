# =============================================================================
# Terraform Variables for Multiplayer Game Server Cloud Infrastructure
# =============================================================================

variable "aws_region" {
  description = "AWS region for deployment"
  type        = string
  default     = "us-east-1"
}

variable "environment" {
  description = "Deployment environment name"
  type        = string
  default     = "production"
}

variable "instance_type" {
  description = "EC2 instance type (t3.micro for AWS Free Tier, c6i.xlarge for production load testing)"
  type        = string
  default     = "t3.micro"
}

variable "key_name" {
  description = "Name of the AWS SSH key pair"
  type        = string
  default     = "game-server-key"
}

variable "allowed_ssh_cidr" {
  description = "CIDR block allowed for SSH management"
  type        = string
  default     = "0.0.0.0/0"
}
