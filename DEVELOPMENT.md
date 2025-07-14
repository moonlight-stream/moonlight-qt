# Artemis Qt Development Guide

## Overview

Artemis Qt is a cross-platform fork of Moonlight Qt, designed to bring the enhanced features from Artemis Android to desktop platforms. This will enable Steam Deck and other desktop/handheld users to benefit from the advanced features originally available only on Android.

## Project Goals

Create a Qt-based desktop version of Artemis with the following features from the Android version:

### Phase 1 (Foundation)
- [ ] Clipboard sync
- [ ] Server commands
- [ ] OTP pairing

### Phase 2 (Client Controls)
- [ ] Client-side fractional refresh rate settings
- [ ] Client-side resolution scaling settings
- [ ] Client-side virtual display control

### Phase 3 (Advanced)
- [ ] App ordering without compatibility mode
- [ ] Client-side permission viewing
- [ ] Input-only mode

## Development Environment Setup ✅

### Local Development (macOS)
```bash
# Install Qt 6 and dependencies
brew install qt6 ffmpeg opus sdl2 sdl2_ttf create-dmg

# Clone and build
git clone https://github.com/wjbeckett/artemis.git
cd artemis
git submodule update --init --recursive
qmake6 moonlight-qt.pro CONFIG+=release
make -j$(sysctl -n hw.ncpu)
```

### Build Artifacts
- **macOS**: `app/Moonlight.app/` - Native macOS application bundle
- **Linux**: Binary executable
- **Windows**: `.exe` with dependencies

## CI/CD Pipeline ✅

### GitHub Actions Workflows

1. **`.github/workflows/test.yml`** - Quick build verification
   - Runs on push/PR to main/develop branches
   - Tests macOS and Linux builds
   - Verifies compilation succeeds

2. **`.github/workflows/build.yml`** - Full release pipeline
   - Multi-platform builds (Windows x64/ARM64, macOS, Linux, Steam Link)
   - Automated artifact uploading
   - Release asset distribution

### Supported Platforms
- **Windows**: x64 and ARM64 architectures
- **macOS**: Universal binary
- **Linux**: AppImage for broad compatibility
- **Steam Link**: Dedicated build for Steam Link devices

## Architecture

### Core Components
- **Qt 6.7+** - Modern UI framework with multimedia support
- **FFmpeg** - Video/audio decoding
- **SDL2** - Input handling and audio
- **OpenSSL** - Encryption and security
- **Opus** - Audio codec

### Key Directories
- `app/` - Main application code
- `backend/` - Core networking and protocol handling
- `gui/` - QML user interface components
- `streaming/` - Audio/video streaming logic
- `settings/` - Configuration management

## Next Steps

### 1. Branding and Identity
```bash
# Update application name and icons
- Change "Moonlight" to "Artemis" in source files
- Update application bundle identifiers
- Replace icons and branding assets
- Update version information
```

### 2. Feature Implementation

#### Starting Point: Clipboard Sync
This is likely the easiest feature to implement first:

1. **Research Android Implementation**
   - Examine `artemis-android` codebase for clipboard sync
   - Identify protocol extensions needed
   - Understand Apollo server requirements

2. **Qt Implementation**
   - Use `QClipboard` for system clipboard access
   - Implement protocol messages for sync
   - Add UI controls in settings

3. **Testing Strategy**
   - Local testing with Apollo server
   - Cross-platform clipboard testing
   - Security and privacy considerations

### 3. Development Workflow

#### Feature Development Process
1. Create feature branch from `develop`
2. Implement changes with tests
3. Update documentation
4. Submit PR for review
5. Merge to `develop` when ready
6. Release from `develop` to `main`

#### Branch Strategy
- `main` - Stable releases only
- `develop` - Active development
- `feature/*` - Individual features
- `hotfix/*` - Critical fixes

### 4. Community and Collaboration

#### Repository Structure
- Clear issue templates for bug reports and features
- Contributing guidelines
- Code of conduct
- Documentation for contributors

#### Relationship with Upstream
- Track changes in both moonlight-qt and artemis-android
- Selective merging of relevant upstream improvements
- Maintain independence for Artemis-specific features

## Resources

### Reference Repositories
- **Artemis Android**: https://github.com/ClassicOldSong/moonlight-android
- **Apollo Server**: https://github.com/ClassicOldSong/Apollo
- **Moonlight Qt**: https://github.com/moonlight-stream/moonlight-qt

### Documentation
- Qt 6 Documentation: https://doc.qt.io/qt-6/
- GameStream Protocol: Available in moonlight-common-c
- SDL2 Documentation: https://wiki.libsdl.org/

## Current Status

✅ **Completed**
- Development environment setup
- Multi-platform CI/CD pipeline
- Build verification on all target platforms
- Repository structure and foundation

🚧 **In Progress**
- Initial project documentation
- Development workflow establishment

📋 **Next Priority**
- Application rebranding
- Clipboard sync implementation
- Feature development infrastructure

---

*For questions or contributions, please open an issue on the GitHub repository.*
