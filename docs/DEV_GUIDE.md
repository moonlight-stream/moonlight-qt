# Artemis Qt Development Guide

This guide will help you set up the development environment and understand how to implement the Artemis features.

## 🎯 **Implementation Status Update**

### ✅ **Foundation Complete** (December 2024)
The core Artemis protocol implementations are now complete and ready for integration:

#### **ClipboardManager** - Real HTTP Protocol ✅
- **Endpoint**: `/actions/clipboard?type=text` (GET/POST)
- **Smart Sync**: Auto-upload on stream start/resume, auto-download on focus loss
- **Loop Prevention**: SHA-256 content hashing to prevent infinite sync loops
- **Settings**: All Android preferences implemented
- **File**: `app/backend/clipboardmanager.{h,cpp}`

#### **OTPPairingManager** - SHA-256 Authentication ✅
- **Hash**: `SHA256(pin + salt + passphrase)` for `&otpauth=` parameter
- **Validation**: 4-digit PIN format validation
- **Detection**: Apollo server capability checking
- **File**: `app/backend/otppairingmanager.{h,cpp}`

#### **ServerCommandManager** - Permission System ✅
- **Permission**: `server_cmd` flag checking (Apollo only)
- **Detection**: Apollo vs standard GameStream server detection
- **Framework**: Command execution and error handling
- **File**: `app/backend/servercommandmanager.{h,cpp}`

### 🔄 **Next: Integration Phase**
Ready to integrate with existing Moonlight Qt components.

## 🌿 Git Workflow & Branching Strategy

### Branch Structure
- **`main`** - Production-ready releases only
- **`development`** - Integration branch for completed features
- **`feature/*`** - Individual feature development branches
- **`fix/*`** - Bug fix branches
- **`hotfix/*`** - Critical production fixes

### Development Workflow
```bash
# 1. Create feature branch from development
git checkout development
git pull origin development
git checkout -b feature/clipboard-ui-integration

# 2. Develop and commit changes
git add .
git commit -m "feat: add clipboard sync UI components"

# 3. Push feature branch
git push origin feature/clipboard-ui-integration

# 4. Create PR: feature/clipboard-ui-integration → development
# 5. After review and merge, create PR: development → main
```

### Branch Naming Convention
- `feature/clipboard-ui-integration`
- `feature/otp-pairing-dialog`
- `feature/server-commands-menu`
- `fix/clipboard-sync-loop`
- `hotfix/critical-crash-fix`

### Commit Message Format
```
type(scope): description

feat(clipboard): add smart sync settings UI
fix(otp): resolve PIN validation edge case
docs(readme): update installation instructions
test(clipboard): add unit tests for sync logic
```

---

## 🔍 Understanding Artemis Features

To properly implement the Artemis features, we need to study the original implementations:

### 1. Examine Artemis Android Source
```bash
# Clone the Artemis Android repository
git clone https://github.com/ClassicOldSong/moonlight-android.git artemis-android
cd artemis-android

# Look for key implementation files
find . -name "*.java" -o -name "*.kt" | grep -i clipboard
find . -name "*.java" -o -name "*.kt" | grep -i command
find . -name "*.java" -o -name "*.kt" | grep -i otp
find . -name "*.java" -o -name "*.kt" | grep -i pairing
```

### 2. Study Apollo Server Protocol
```bash
# Clone Apollo server to understand the protocol extensions
git clone https://github.com/ClassicOldSong/Apollo.git
cd Apollo

# Look for protocol documentation or implementation
find . -name "*.md" -o -name "*.txt" | xargs grep -l -i "clipboard\|command\|otp"
```

### 3. Key Areas to Investigate

#### Clipboard Sync
- **Android Location**: Look for clipboard-related files in `app/src/main/java/`
- **Protocol**: How does the client communicate clipboard data to the server?
- **Format**: What data formats are supported (text, images, files)?
- **Security**: How is clipboard data secured during transmission?

#### Server Commands
- **Android Location**: Search for "command" or "server command" in the codebase
- **Protocol**: What HTTP endpoints or GameStream extensions are used?
- **Commands**: What types of commands are supported?
- **UI**: How are commands presented to the user?

#### OTP Pairing
- **Android Location**: Look for OTP, pairing, or authentication code
- **Protocol**: How does OTP pairing differ from standard PIN pairing?
- **Security**: What cryptographic methods are used?
- **Flow**: What's the complete pairing workflow?

## 🛠️ Development Setup

### Prerequisites
- Qt 6.7+ with QML support
- C++ compiler (GCC, Clang, or MSVC)
- Git with submodules support
- Platform-specific dependencies (see main README)

### Quick Setup Script
```bash
# Use the provided setup script
chmod +x scripts/setup-dev.sh
./scripts/setup-dev.sh
```

### Manual Setup
```bash
# 1. Clone with submodules
git clone --recursive https://github.com/wjbeckett/artemis.git
cd artemis

# 2. Install Qt dependencies (example for Ubuntu)
sudo apt update
sudo apt install qt6-base-dev qt6-declarative-dev libqt6svg6-dev \
  qml6-module-qtquick-controls qml6-module-qtquick-templates \
  qml6-module-qtquick-layouts

# 3. Install media dependencies
sudo apt install libavcodec-dev libavformat-dev libswscale-dev \
  libsdl2-dev libsdl2-ttf-dev libopus-dev libssl-dev

# 4. Build
qmake6 moonlight-qt.pro CONFIG+=debug
make -j$(nproc)
```

## 📁 Project Structure

```
artemis/
├── app/
│   ├── backend/           # Artemis feature implementations
│   │   ├── clipboardmanager.*     # Clipboard sync
│   │   ├── servercommandmanager.* # Server commands
│   │   └── otppairingmanager.*    # OTP pairing
│   ├── settings/          # Settings management
│   │   └── artemissettings.*      # Artemis-specific settings
│   └── gui/               # UI components
├── scripts/               # Build and setup scripts
├── .github/workflows/     # CI/CD pipelines
└── docs/                  # Documentation
```

## 🐛 Debugging Tips

### Enable Debug Logging
```cpp
// In your implementation files
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(artemisClipboard, "artemis.clipboard")
Q_LOGGING_CATEGORY(artemisCommands, "artemis.commands")
Q_LOGGING_CATEGORY(artemisOTP, "artemis.otp")

// Use in code
qCDebug(artemisClipboard) << "Clipboard sync started";
```

### Run with Debug Output
```bash
# Enable all Artemis debug categories
QT_LOGGING_RULES="artemis.*=true" ./artemis

# Enable specific categories
QT_LOGGING_RULES="artemis.clipboard=true" ./artemis
```

### Network Debugging
```bash
# Monitor network traffic to understand protocols
sudo tcpdump -i any -w artemis-traffic.pcap host <server-ip>
wireshark artemis-traffic.pcap
```

## 📚 Resources

### Documentation
- [Qt 6 Documentation](https://doc.qt.io/qt-6/)
- [GameStream Protocol](https://github.com/moonlight-stream/moonlight-docs/wiki/GameStream-Protocol)
- [Moonlight Qt Architecture](https://github.com/moonlight-stream/moonlight-qt/wiki)

### Related Projects
- [Artemis Android](https://github.com/ClassicOldSong/moonlight-android)
- [Apollo Server](https://github.com/ClassicOldSong/Apollo)
- [Sunshine Server](https://github.com/LizardByte/Sunshine)

### Community
- [Moonlight Discord](https://moonlight-stream.org/discord)
- [GitHub Discussions](https://github.com/wjbeckett/artemis/discussions)

## 🤝 Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for detailed contribution guidelines.

Remember: The foundation code I provided is a starting point based on assumptions. The real implementation should be based on studying the actual Artemis Android source code and Apollo server protocols.