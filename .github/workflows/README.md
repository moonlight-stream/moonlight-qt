# GitHub Actions 工作流说明

本项目使用 GitHub Actions 进行自动化构建和部署。

## 工作流概述

### 主构建工作流 (`build.yml`)

**触发条件：**
- 推送到 `master` 或 `main` 分支
- 创建 Pull Request
- 发布 Release

**支持平台：**
- Windows (x64/ARM64) - 使用 Qt 6.11.1
- macOS - 使用 Qt 6.11.1
- Linux (x86_64 / aarch64) - 使用 Qt 6.11.1
- SteamLink - 交叉编译

**构建产物：**
- Windows: `Moonlight-VPlus-Portable-{arch}-r{build_number}.zip`
- macOS: `Moonlight-VPlus-r{build_number}.dmg`
- Linux: `Moonlight-VPlus-r{build_number}-{arch}.AppImage`
- SteamLink: `Moonlight-VPlus-SteamLink-r{build_number}.zip`

首个 V+ 品牌版本还会同时发布 `MoonlightPortable-*` 和 `MoonlightSetup-*`
兼容别名，保证旧版 Windows 客户端能完成跨品牌升级。

## 平台特定配置

### Windows 构建
- 使用 Visual Studio 2022
- 支持 x64 和 ARM64 架构
- 自动处理 Qt 依赖部署
- 生成调试符号包

### macOS 构建
- 使用 macOS 14 (Sonoma)
- 生成 DMG 安装包
- 需要 Node.js 和 create-dmg

### Linux 构建
- 使用 Ubuntu 22.04
- 编译多个依赖库：SDL2, SDL2_ttf, libva, dav1d, libplacebo, FFmpeg
- 智能检测 Wayland 支持
- 生成 AppImage 格式

### SteamLink 构建
- 使用 Valve SteamLink SDK
- 交叉编译为 ARM 架构

## 使用说明

### 自动构建
每次推送代码或创建 PR 时，会自动触发构建。

### 发布版本
在 GitHub 上创建新的 Release 时，会自动构建并上传所有平台的构建产物。

### 查看构建状态
- 在仓库主页查看构建状态
- 在 Actions 页面查看详细日志
- 构建产物在 Artifacts 部分下载

## 故障排除

### 常见问题

1. **Windows 构建超时**：
   - 检查是否有 "Terminate batch job (Y/N)?" 提示
   - 构建脚本会自动清理环境变量避免冲突

2. **Linux 包缺失**：
   - 确保使用正确的包名（Ubuntu 22.04）
   - Wayland 包会自动检测，不可用时会跳过

3. **Qt 版本问题**：
   - Windows/macOS 使用 Qt 6.x
   - Linux 使用 CI 安装的 Qt 6.x

4. **依赖库构建失败**：
   - 检查网络连接（需要下载源码）
   - 某些可选依赖失败不会影响主构建

### 环境要求

- **Windows**: Visual Studio 2022, Qt 6.11.1
- **macOS**: Xcode, Qt 6.11.1, Node.js
- **Linux**: 完整的开发环境，Qt 6.11.1
- **所有平台**: Git, 网络访问

### 密钥配置

需要在仓库设置中配置：
- `GH_BOT_TOKEN`: GitHub 个人访问令牌（用于 Release 上传）

## 自定义配置

### 修改 Qt 版本
编辑工作流文件中的 `install-qt-action` 版本参数。

### 添加新平台
在 `strategy.matrix` 中添加新的配置组合。

### 修改构建脚本
更新对应平台的构建步骤或脚本文件。

## 技术细节

### 构建缓存
- Qt 安装使用缓存加速
- 依赖库编译结果不缓存（确保最新版本）

### 并行构建
- Windows: 自动检测并行度
- Linux: 使用 `$(nproc)` 自动检测
- macOS: 默认并行构建

### 错误处理
- 自动重试机制
- 详细的错误日志
- 优雅的失败处理

这个工作流设计为开箱即用，无需额外配置即可在 GitHub Actions 中运行。
