#!/bin/bash

# Configuration
INSTALL_DIR="/usr/local/bin"
SERVICE_FILE="/etc/systemd/system/gridflux.service"
USER_SERVICE_FILE="$HOME/.config/systemd/user/gridflux.service"
BINARY_NAME="gridflux"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Print with color
print_status() {
  echo -e "${GREEN}[*]${NC} $1"
}

print_warning() {
  echo -e "${YELLOW}[!]${NC} $1"
}

print_error() {
  echo -e "${RED}[x]${NC} $1"
}

# Check if running as root
if [ "$EUID" -eq 0 ]; then
  print_error "Please do not run as root or with sudo"
  exit 1
fi

# Function to stop and remove the systemd service
remove_systemd_service() {
  print_status "Stopping and removing systemd service..."

  # Stop and disable system-wide service
  if [ -f "$SERVICE_FILE" ]; then
    print_status "Removing system-wide service..."
    sudo systemctl stop gridflux.service 2>/dev/null || true
    sudo systemctl disable gridflux.service 2>/dev/null || true
    sudo rm -f "$SERVICE_FILE"
    sudo systemctl daemon-reload
  fi

  # Stop and disable user service
  if [ -f "$USER_SERVICE_FILE" ]; then
    print_status "Removing user service..."
    systemctl --user stop gridflux.service 2>/dev/null || true
    systemctl --user disable gridflux.service 2>/dev/null || true
    rm -f "$USER_SERVICE_FILE"
    systemctl --user daemon-reload
  fi
}

# Function to remove the binary
remove_binary() {
  print_status "Removing gridflux binary..."

  # Kill any running instances
  print_status "Stopping any running instances of gridflux..."
  pkill gridflux 2>/dev/null || true

  # Wait a moment for processes to stop
  sleep 2

  # Remove the binary
  if [ -f "$INSTALL_DIR/$BINARY_NAME" ]; then
    sudo rm -f "$INSTALL_DIR/$BINARY_NAME"
    print_status "Removed $INSTALL_DIR/$BINARY_NAME"
  else
    print_warning "Binary not found in $INSTALL_DIR"
  fi
}

# Function to clean build artifacts
clean_build() {
  print_status "Cleaning build artifacts..."
  if [ -f "Makefile" ]; then
    make clean 2>/dev/null || true
  fi
  rm -f gridflux 2>/dev/null || true
}

# Main uninstall function
main() {
  print_status "Starting gridflux uninstallation..."

  remove_systemd_service
  remove_binary
  clean_build

  print_status "Uninstallation complete!"
  print_warning "Note: Dependencies were not removed to avoid conflicts with other applications."
  print_status "If you want to remove dependencies, please use your package manager manually."
}

# Ask for confirmation
echo -e "${YELLOW}This will completely remove gridflux from your system.${NC}"
echo -n "Do you want to continue? [y/N] "
read -r response

case "$response" in
[yY][eE][sS] | [yY])
  main
  ;;
*)
  print_warning "Uninstallation cancelled."
  exit 0
  ;;
esac
