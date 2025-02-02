CC = gcc
CFLAGS = `pkg-config --cflags gtk+-3.0 libwnck-3.0`
LDFLAGS = `pkg-config --libs gtk+-3.0 libwnck-3.0` -lxcb

SRC_DIR = src
BUILD_DIR = build

SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC))

EXEC = minewm  

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
SYSTEMD_DIR = $(HOME)/.config/systemd/user  

all: $(BUILD_DIR) $(EXEC)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(EXEC): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) -c $< -o $@ $(CFLAGS)

# Install dependencies based on the package manager
install-deps:
	@echo "Detecting package manager and installing dependencies..."
	@if [ -f /etc/debian_version ]; then \
		sudo apt update && sudo apt install -y gcc make pkg-config libgtk-3-dev libwnck-3-dev libxcb1-dev; \
	elif [ -f /etc/fedora-release ]; then \
		sudo dnf install -y gcc make pkg-config gtk3-devel libwnck3-devel libxcb-devel; \
	elif [ -f /etc/arch-release ]; then \
		sudo pacman -Sy --noconfirm gcc make pkg-config gtk3 libwnck3 xcb; \
	elif [ -f /etc/alpine-release ]; then \
		sudo apk add --no-cache gcc make pkgconf gtk+3.0-dev libwnck-dev libxcb-dev; \
	else \
		echo "Unsupported Linux distribution. Please install dependencies manually."; \
		exit 1; \
	fi
	@echo "Dependencies installed successfully."

# Install the program and enable systemd service (default install)
install: install-deps $(EXEC) install-systemd

# Install the binary
install-bin:
	install -d $(BINDIR)          
	install -m 755 $(EXEC) $(BINDIR)/$(EXEC)
	@echo "Installed $(EXEC) to $(BINDIR)"

# Install systemd service for user session
install-systemd: install-bin
	install -d $(SYSTEMD_DIR)
	echo "[Unit]\n\
	Description=MineWM Window Manager\n\
	After=graphical-session.target\n\
	\n\
	[Service]\n\
	ExecStart=$(BINDIR)/$(EXEC)\n\
	Restart=always\n\
	\n\
	[Install]\n\
	WantedBy=default.target" > $(SYSTEMD_DIR)/minewm.service
	@echo "Systemd service installed at $(SYSTEMD_DIR)/minewm.service"
	systemctl --user daemon-reload
	systemctl --user enable minewm.service
	systemctl --user start minewm.service
	@echo "MineWM service started successfully and will run at login."

# Uninstall the program and remove systemd service
uninstall:
	systemctl --user stop minewm.service || true
	systemctl --user disable minewm.service || true
	rm -f $(SYSTEMD_DIR)/minewm.service
	systemctl --user daemon-reload
	rm -f $(BINDIR)/$(EXEC)
	@echo "Uninstalled $(EXEC) and removed minewm service."

clean:
	rm -rf $(BUILD_DIR) $(EXEC)

# Phony targets
.PHONY: all clean install install-deps install-systemd uninstall
