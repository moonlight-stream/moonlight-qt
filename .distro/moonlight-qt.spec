%global         forgeurl0 https://github.com/moonlight-stream/moonlight-qt

# TODO: Debundle these once the build-system can do that
%global         forgeurl1 https://github.com/moonlight-stream/moonlight-common-c
%global         forgeurl2 https://github.com/cgutman/qmdnsengine
%global         forgeurl3 https://github.com/gabomdq/SDL_GameControllerDB
%global         forgeurl4 https://github.com/aizvorski/h264bitstream
%global         forgeurl5 https://github.com/cgutman/enet
%global         forgeurl6 https://github.com/sleepybishop/nanors

%global         version0  0.0.0

%global         commit1   PLACEHOLDER
%global         commit2   PLACEHOLDER
%global         commit3   PLACEHOLDER
%global         commit4   PLACEHOLDER
%global         commit5   PLACEHOLDER
%global         commit6   PLACEHOLDER

Provides:       bundled(moonlight-common-c) = 0~git%{commit1}
Provides:       bundled(qmdnsengine) = 0.1.0^git%{commit2}
Provides:       bundled(SDL_GameControllerDB) = 0~git%{commit3}
Provides:       bundled(h264bitstream) = 0.2.0^git%{commit4}
Provides:       bundled(enet) = 1.3.17^git%{commit5}
Provides:       bundled(nanors-common-c) = 0~git%{commit6}

Name:           moonlight-qt
Release:        %autorelease
%forgemeta -a
Version:        %forgeversion
Summary:        GameStream client for PCs

License:        GPL-3.0-or-later
URL:            %forgeurl0
Source0:        %forgesource0
Source1:        %forgesource1
Source2:        %forgesource2
Source3:        %forgesource3
Source4:        %forgesource4
Source5:        %forgesource5
Source6:        %forgesource6

BuildRequires:  appstream
BuildRequires:  desktop-file-utils
BuildRequires:  gcc-c++
BuildRequires:  make
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  qt6-qtsvg-devel
BuildRequires:  pkgconfig(egl)
BuildRequires:  pkgconfig(gl)
BuildRequires:  pkgconfig(libavcodec) >= 60
BuildRequires:  pkgconfig(libavformat)
BuildRequires:  pkgconfig(libavutil)
BuildRequires:  pkgconfig(libdrm)
BuildRequires:  pkgconfig(libplacebo) >= 7.349.0
BuildRequires:  pkgconfig(libswscale)
BuildRequires:  pkgconfig(libva)
BuildRequires:  pkgconfig(libva-drm)
BuildRequires:  pkgconfig(libva-wayland)
BuildRequires:  pkgconfig(libva-x11)
BuildRequires:  pkgconfig(openssl)
BuildRequires:  pkgconfig(opus)
BuildRequires:  pkgconfig(sdl2)
BuildRequires:  pkgconfig(SDL2_ttf)
BuildRequires:  pkgconfig(vdpau)
BuildRequires:  pkgconfig(wayland-client)
BuildRequires:  pkgconfig(x11)

# Directory ownership
Requires: hicolor-icon-theme

%description
Moonlight is an open-source game-streaming client for Sunshine and NVIDIA
GameStream hosts. This package tracks the upstream master branch and is built
entirely from source, including its pinned Git submodules.


%prep
%autosetup -p1 -n moonlight-qt-%{version}
tar -xf %{SOURCE1} --strip-components=1 -C moonlight-common-c/moonlight-common-c
tar -xf %{SOURCE2} --strip-components=1 -C qmdnsengine/qmdnsengine
tar -xf %{SOURCE3} --strip-components=1 -C app/SDL_GameControllerDB
tar -xf %{SOURCE4} --strip-components=1 -C h264bitstream/h264bitstream
tar -xf %{SOURCE5} --strip-components=1 -C moonlight-common-c/moonlight-common-c/enet
tar -xf %{SOURCE6} --strip-components=1 -C moonlight-common-c/moonlight-common-c/nanors


%conf
%qmake_qt6 \
  "PREFIX=%{_prefix}"


%build
%make_build


%install
%make_install INSTALL_ROOT=%{buildroot}


%check
desktop-file-validate \
    %{buildroot}%{_datadir}/applications/com.moonlight_stream.Moonlight.desktop
appstreamcli validate --no-net \
    %{buildroot}%{_datadir}/metainfo/com.moonlight_stream.Moonlight.appdata.xml


%files
%license LICENSE
%doc README.md
%{_bindir}/moonlight
%{_datadir}/applications/com.moonlight_stream.Moonlight.desktop
%{_datadir}/icons/hicolor/scalable/apps/moonlight.svg
%{_datadir}/metainfo/com.moonlight_stream.Moonlight.appdata.xml


%changelog
%autochangelog
