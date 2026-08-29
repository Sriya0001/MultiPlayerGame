# =============================================================================
# Terraform Main Configuration — AWS Cloud Infrastructure
# =============================================================================

terraform {
  required_version = ">= 1.5.0"
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.0"
    }
  }
}

provider "aws" {
  region = var.aws_region
}

# ── 1. Virtual Private Cloud (VPC) & Networking ──────────────────────────────

resource "aws_vpc" "game_vpc" {
  cidr_block           = "10.0.0.0/16"
  enable_dns_hostnames = true
  enable_dns_support   = true

  tags = {
    Name        = "game-server-vpc"
    Environment = var.environment
  }
}

resource "aws_internet_gateway" "game_igw" {
  vpc_id = aws_vpc.game_vpc.id

  tags = {
    Name = "game-server-igw"
  }
}

resource "aws_subnet" "public_subnet" {
  vpc_id                  = aws_vpc.game_vpc.id
  cidr_block              = "10.0.1.0/24"
  map_public_ip_on_launch = true
  availability_zone       = "${var.aws_region}a"

  tags = {
    Name = "game-server-public-subnet"
  }
}

resource "aws_route_table" "public_rt" {
  vpc_id = aws_vpc.game_vpc.id

  route {
    cidr_block = "0.0.0.0/0"
    gateway_id = aws_internet_gateway.game_igw.id
  }

  tags = {
    Name = "game-server-public-rt"
  }
}

resource "aws_route_table_association" "public_assoc" {
  subnet_id      = aws_subnet.public_subnet.id
  route_table_id = aws_route_table.public_rt.id
}

# ── 2. Security Group / Cloud Firewall ───────────────────────────────────────

resource "aws_security_group" "game_server_sg" {
  name        = "game-server-sg"
  description = "Security group for multiplayer game server and observability"
  vpc_id      = aws_vpc.game_vpc.id

  # SSH Management
  ingress {
    description = "SSH Access"
    from_port   = 22
    to_port     = 22
    protocol    = "tcp"
    cidr_blocks = [var.allowed_ssh_cidr]
  }

  # Game Server Gameplay Protocol
  ingress {
    description = "Game Server TCP Protocol"
    from_port   = 7777
    to_port     = 7777
    protocol    = "tcp"
    cidr_blocks = ["0.0.0.0/0"]
  }

  # Web Visual Arena Dashboard
  ingress {
    description = "Visual Game Web Dashboard"
    from_port   = 8080
    to_port     = 8080
    protocol    = "tcp"
    cidr_blocks = ["0.0.0.0/0"]
  }

  # Prometheus Metrics Exporter
  ingress {
    description = "Prometheus Metrics Endpoint"
    from_port   = 9100
    to_port     = 9100
    protocol    = "tcp"
    cidr_blocks = ["0.0.0.0/0"]
  }

  # Prometheus UI
  ingress {
    description = "Prometheus Web UI"
    from_port   = 9090
    to_port     = 9090
    protocol    = "tcp"
    cidr_blocks = ["0.0.0.0/0"]
  }

  # Grafana Dashboards
  ingress {
    description = "Grafana Observability Portal"
    from_port   = 3000
    to_port     = 3000
    protocol    = "tcp"
    cidr_blocks = ["0.0.0.0/0"]
  }

  # Outbound Internet
  egress {
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
    cidr_blocks = ["0.0.0.0/0"]
  }

  tags = {
    Name = "game-server-sg"
  }
}

# ── 3. Ubuntu 24.04 LTS AMI Lookup ──────────────────────────────────────────

data "aws_ami" "ubuntu" {
  most_recent = true
  filter {
    name   = "name"
    values = ["ubuntu/images/hvm-ssd-gp3/ubuntu-noble-24.04-amd64-server-*"]
  }
  filter {
    name   = "virtualization-type"
    values = ["hvm"]
  }
  owners = ["099720109477"] # Canonical
}

# ── 4. Compute Instances ─────────────────────────────────────────────────────

resource "aws_instance" "game_server" {
  ami                         = data.aws_ami.ubuntu.id
  instance_type               = var.instance_type
  subnet_id                   = aws_subnet.public_subnet.id
  vpc_security_group_ids      = [aws_security_group.game_server_sg.id]
  key_name                    = var.key_name
  associate_public_ip_address = true

  root_block_device {
    volume_size = 20
    volume_type = "gp3"
  }

  tags = {
    Name        = "multiplayer-game-server-node"
    Role        = "game-server"
    Environment = var.environment
  }
}
