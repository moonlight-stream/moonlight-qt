#!/bin/bash

# Artemis Qt Development Setup Script
# This script helps set up the development environment for Artemis Qt

set -e

echo "🚀 Setting up Artemis Qt development environment..."

# Detect OS
OS="unknown"
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]]; then
    OS="windows"
fi

echo "📋 Detected OS: $OS"

# Function to install dependencies on different platforms
install_dependencies() {
    case $OS in
        "linux")
            echo "📦 Installing Linux dependencies..."
            if command -v apt-get &> /dev/null; then
                # Debian/Ubuntu
                sudo apt-get update
                sudo apt-get install -y \
                    qt6-base-dev qt6-declarative-dev libqt6svg6-dev \
                    qml6-module-qtquick-controls qml6-module-qtquick-templates \
                    qml6-module-qtquick-layouts qml6-module-qtqml-workerscript \
                    qml6-module-qtquick-window qml6-module-qtquick \
                    libegl1-mesa-dev libgl1-mesa-dev libopus-dev libsdl2-dev \
                    libsdl2-ttf-dev libssl-dev libavcodec-dev libavformat-dev \
                    libswscale-dev libva-dev libvdpau-dev libxkbcommon-dev \
                    wayland-protocols libdrm-dev build-essential git
            elif command -v dnf &> /dev/null; then
                # Fedora/RHEL
                sudo dnf install -y \
                    qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtsvg-devel \
                    openssl-devel SDL2-devel SDL2_ttf-devel ffmpeg-devel \
                    libva-devel libvdpau-devel opus-devel pulseaudio-libs-devel \
                    alsa-lib-devel libdrm-devel gcc-c++ git
            else
                echo "❌ Unsupported Linux distribution. Please install dependencies manually."
                exit 1
            fi
            ;;
        "macos")
            echo "📦 Installing macOS dependencies..."
            if ! command -v brew &> /dev/null; then
                echo "❌ Homebrew not found. Please install Homebrew first:"
                echo "   /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
                exit 1
            fi
            brew install qt6 ffmpeg opus sdl2 sdl2_ttf create-dmg
            ;;
        "windows")
            echo "📦 Windows setup requires manual installation:"
            echo "   1. Install Qt 6.7+ from https://www.qt.io/download"
            echo "   2. Install Visual Studio 2022 with MSVC"
            echo "   3. Install 7-Zip for packaging"
            echo "   4. Ensure Qt's bin directory is in your PATH"
            ;;
        *)
            echo "❌ Unsupported operating system: $OS"
            exit 1
            ;;
    esac
}

# Function to initialize submodules
init_submodules() {
    echo "📥 Initializing git submodules..."
    git submodule update --init --recursive
}

# Function to build the project
build_project() {
    echo "🔨 Building Artemis Qt..."
    
    # Clean any existing build files
    echo "🧹 Cleaning previous builds..."
    rm -f .qmake.stash .qmake.cache
    find . -name "Makefile*" -delete 2>/dev/null || true
    
    case $OS in
        "linux"|"macos")
            if command -v qmake6 &> /dev/null; then
                QMAKE="qmake6"
            elif command -v qmake &> /dev/null; then
                QMAKE="qmake"
            else
                echo "❌ qmake not found. Please ensure Qt is installed and in your PATH."
                exit 1
            fi
            
            echo "🔧 Running qmake..."
            $QMAKE moonlight-qt.pro CONFIG+=debug
            
            echo "🔨 Compiling..."
            if [[ "$OS" == "macos" ]]; then
                make -j$(sysctl -n hw.ncpu)
            else
                make -j$(nproc)
            fi
            ;;
        "windows")
            echo "🔧 For Windows, use Qt Creator or run from Qt Command Prompt:"
            echo "   qmake6 moonlight-qt.pro CONFIG+=debug"
            echo "   nmake"
            ;;
    esac
}

# Function to create development configuration
create_dev_config() {
    echo "⚙️  Creating development configuration..."
    
    # Create a development settings file
    mkdir -p ~/.config/artemis-dev
    cat > ~/.config/artemis-dev/dev-settings.conf << EOF
# Artemis Qt Development Settings
# This file contains development-specific settings

[Development]
EnableDebugLogging=true
ShowDeveloperOptions=true
EnableExperimentalFeatures=true

[Artemis]
# Artemis-specific settings will go here
ClipboardSyncEnabled=false
ServerCommandsEnabled=false
OTPPairingEnabled=false
EOF

    echo "📝 Development configuration created at ~/.config/artemis-dev/"
}

# Function to show next steps
show_next_steps() {
    echo ""
    echo "✅ Development environment setup complete!"
    echo ""
    echo "🎯 Next steps:"
    echo "   1. Start implementing Artemis features (see DEVELOPMENT.md)"
    echo "   2. Run the application:"
    
    case $OS in
        "linux")
            echo "      ./app/moonlight"
            ;;
        "macos")
            echo "      open app/Moonlight.app"
            ;;
        "windows")
            echo "      app\\debug\\moonlight.exe"
            ;;
    esac
    
    echo "   3. Begin with Phase 1 features:"
    echo "      - Clipboard sync"
    echo "      - Server commands"
    echo "      - OTP pairing"
    echo ""
    echo "📚 Documentation:"
    echo "   - Development guide: DEVELOPMENT.md"
    echo "   - Contributing: CONTRIBUTING.md"
    echo "   - Artemis Android reference: https://github.com/ClassicOldSong/moonlight-android"
    echo ""
}

# Main execution
main() {
    echo "🎮 Artemis Qt Development Setup"
    echo "================================"
    echo ""
    
    # Check if we're in the right directory
    if [[ ! -f "moonlight-qt.pro" ]]; then
        echo "❌ Error: moonlight-qt.pro not found."
        echo "   Please run this script from the root of the Artemis Qt repository."
        exit 1
    fi
    
    # Parse command line arguments
    SKIP_DEPS=false
    SKIP_BUILD=false
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            --skip-deps)
                SKIP_DEPS=true
                shift
                ;;
            --skip-build)
                SKIP_BUILD=true
                shift
                ;;
            --help|-h)
                echo "Usage: $0 [options]"
                echo ""
                echo "Options:"
                echo "  --skip-deps   Skip dependency installation"
                echo "  --skip-build  Skip building the project"
                echo "  --help, -h    Show this help message"
                exit 0
                ;;
            *)
                echo "❌ Unknown option: $1"
                echo "   Use --help for usage information."
                exit 1
                ;;
        esac
    done
    
    # Execute setup steps
    if [[ "$SKIP_DEPS" != true ]]; then
        install_dependencies
    fi
    
    init_submodules
    create_dev_config
    
    if [[ "$SKIP_BUILD" != true ]]; then
        build_project
    fi
    
    show_next_steps
}

# Run main function
main "$@"