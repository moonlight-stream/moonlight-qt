#!/usr/bin/env python3

import argparse
import os
import platform
import shutil
import subprocess
from pathlib import Path


def run(command):
    print("+", " ".join(str(argument) for argument in command), flush=True)
    subprocess.run(command, check=True)


def configure_and_install(source_dir, build_dir, install_root, install_libdir,
                          install_bindir, install_includedir, extra_arguments):
    if build_dir.exists():
        shutil.rmtree(build_dir)

    run([
        "cmake",
        "-S", source_dir,
        "-B", build_dir,
        f"-DCMAKE_INSTALL_PREFIX={install_root}",
        f"-DCMAKE_INSTALL_LIBDIR={install_libdir}",
        f"-DCMAKE_INSTALL_BINDIR={install_bindir}",
        f"-DCMAKE_INSTALL_INCLUDEDIR={install_includedir}",
        *extra_arguments,
    ])
    run([
        "cmake",
        "--build", build_dir,
        "--config", "Release",
        "--target", "install",
        "--parallel",
    ])


def get_visual_studio_generator():
    vswhere = (Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) /
               "Microsoft Visual Studio" / "Installer" / "vswhere.exe")
    result = subprocess.run(
        [vswhere, "-latest", "-products", "*", "-property", "installationVersion"],
        check=True,
        capture_output=True,
        text=True,
    )
    major_version = int(result.stdout.strip().split(".", maxsplit=1)[0])
    generators = {
        17: "Visual Studio 17 2022",
        18: "Visual Studio 18 2026",
    }
    if major_version not in generators:
        raise RuntimeError(f"Unsupported Visual Studio version: {major_version}")
    return generators[major_version]


def build_windows(sdl_source, sdl_ttf_source, build_root, install_root):
    generator = get_visual_studio_generator()
    for architecture, directory_name in (("x64", "x64"), ("ARM64", "ARM64")):
        install_libdir = f"lib/{directory_name}"
        install_includedir = f"include/{directory_name}"
        generator_arguments = ["-G", generator, "-A", architecture]

        configure_and_install(
            sdl_source,
            build_root / f"SDL-{directory_name}",
            install_root,
            install_libdir,
            install_libdir,
            install_includedir,
            [
                *generator_arguments,
                "-DSDL_SHARED=ON",
                "-DSDL_STATIC=OFF",
                "-DSDL_TEST_LIBRARY=OFF",
                "-DSDL_TESTS=OFF",
                "-DSDL_INSTALL_TESTS=OFF",
            ],
        )
        configure_and_install(
            sdl_ttf_source,
            build_root / f"SDL_ttf-{directory_name}",
            install_root,
            install_libdir,
            install_libdir,
            install_includedir,
            [
                *generator_arguments,
                f"-DSDL3_DIR={install_root / 'cmake'}",
                "-DSDLTTF_VENDORED=ON",
                "-DSDLTTF_SAMPLES=OFF",
                "-DSDLTTF_TESTS=OFF",
            ],
        )


def build_macos(sdl_source, sdl_ttf_source, build_root, install_root):
    common_arguments = [
        "-DCMAKE_OSX_ARCHITECTURES=x86_64;arm64",
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=13",
    ]
    configure_and_install(
        sdl_source,
        build_root / "SDL-macos",
        install_root,
        "lib",
        "lib",
        "include",
        [
            *common_arguments,
            "-DSDL_SHARED=ON",
            "-DSDL_STATIC=OFF",
            "-DSDL_TEST_LIBRARY=OFF",
            "-DSDL_TESTS=OFF",
            "-DSDL_INSTALL_TESTS=OFF",
        ],
    )
    configure_and_install(
        sdl_ttf_source,
        build_root / "SDL_ttf-macos",
        install_root,
        "lib",
        "lib",
        "include",
        [
            *common_arguments,
            f"-DSDL3_DIR={install_root / 'lib' / 'cmake' / 'SDL3'}",
            "-DSDLTTF_VENDORED=ON",
            "-DSDLTTF_SAMPLES=OFF",
            "-DSDLTTF_TESTS=OFF",
        ],
    )


def main():
    repository_root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(
        description="Build the SDL 3 dependencies required by Moonlight",
    )
    parser.add_argument(
        "--sdl-source",
        type=Path,
        default=repository_root / "deps" / "SDL",
        help="SDL source checkout",
    )
    parser.add_argument(
        "--sdl-ttf-source",
        type=Path,
        default=repository_root / "deps" / "SDL_ttf",
        help="SDL_ttf source checkout",
    )
    args = parser.parse_args()

    sdl_source = args.sdl_source.resolve()
    sdl_ttf_source = args.sdl_ttf_source.resolve()
    if not (sdl_source / "CMakeLists.txt").is_file():
        parser.error(f"Missing SDL source checkout: {sdl_source}")
    if not (sdl_ttf_source / "CMakeLists.txt").is_file():
        parser.error(f"Missing SDL_ttf source checkout: {sdl_ttf_source}")

    build_root = repository_root / "build" / "sdl3-deps"
    system = platform.system()
    if system == "Windows":
        build_windows(sdl_source, sdl_ttf_source, build_root,
                      repository_root / "libs" / "windows")
    elif system == "Darwin":
        build_macos(sdl_source, sdl_ttf_source, build_root,
                    repository_root / "libs" / "mac")
    else:
        parser.error(f"Unsupported platform: {system}")


if __name__ == "__main__":
    main()
