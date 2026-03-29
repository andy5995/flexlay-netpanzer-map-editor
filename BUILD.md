# Build Instructions

## Easiest Way: AppImage via Docker

The easiest way to build is using the `andy5995/linuxdeploy:v3-jammy` container
(available on Docker Hub), which has all build tools and linuxdeploy pre-installed.
This produces a self-contained AppImage in `out/`.

```sh
docker compose up
```

The AppImage requires `ruby` on the host system at runtime.

> [!CAUTION]
> The editor **will not function** without the netpanzer game data directory.
> You must set `NETPANZER_DATADIR` to its location before launching:
>
> ```sh
> NETPANZER_DATADIR=/path/to/netpanzer ./out/netpanzer-editor-0.1-x86_64.AppImage
> ```
>
> The game data is not bundled in the AppImage and is not part of this
> repository. It is typically installed with the netpanzer game package
> (e.g. `sudo apt install netpanzer`) or built from source.

### Testing changes to `make-appimage.sh`

Use `docker-compose.dev.yml` to get an interactive shell inside the container:

```sh
docker compose -f docker-compose.dev.yml run --rm build
```

From there you can run `./make-appimage.sh` directly or step through it manually.

---

## Prerequisites

### System Requirements
- Linux (tested on Ubuntu 22.04+)
- x86_64 architecture
- OpenGL 1.2+ capable GPU with working drivers

### Dependencies

Install build tools and development libraries:

**Ubuntu/Debian:**
```bash
sudo apt install build-essential libboost-dev ruby-dev scons swig \
    libx11-dev libxmu-dev libxi-dev libxxf86vm-dev libxrandr-dev \
    libgl1-mesa-dev libglu1-mesa-dev libpng-dev libjpeg-dev
```

**Fedora/CentOS/RHEL:**
```bash
sudo yum groupinstall "Development Tools"
sudo yum install boost-devel ruby-devel scons swig \
    libX11-devel libXmu-devel libXi-devel libXxf86vm-devel libXrandr-devel \
    mesa-libGL-devel mesa-libGLU-devel libpng-devel libjpeg-turbo-devel
```

**X11 Dependencies:**
The project requires several X11 extension libraries:
- **libx11-dev/libX11-devel** - X11 core protocol
- **libxmu-dev/libXmu-devel** - X11 Miscellaneous Utilities
- **libxi-dev/libXi-devel** - X11 Input extensions (XInput)
- **libxxf86vm-dev/libXxf86vm-devel** - X11 Video Mode extensions
- **libxrandr-dev/libXrandr-devel** - X11 Resize and Rotate extensions

## Building

### Quick Start
```bash
scons
```

This is the primary build command. It will:
1. Build the bundled ClanLib library (external/clanlib/)
2. Compile Flexlay core library
3. Generate Ruby bindings for game editors
4. Create shared objects for game-specific modules

### Build Output

After successful compilation, you will have:
- `lib/libflexlay.a` - Main Flexlay library
- `ruby/flexlay_wrap.so` - Ruby bindings for Flexlay
- `netpanzer/netpanzer_wrap.so` - NetPanzer editor bindings
- Editor executables in `netpanzer/`, `supertux/`, `paint/` directories

### Rebuilding ClanLib

The bundled ClanLib has been patched for modern C++ compatibility. To rebuild it:

```bash
cd external/clanlib
scons -c    # Clean previous build
scons       # Rebuild
cd ../..
```

### Build Configuration

The main build files are:
- `SConstruct` - Main build script
- `external/clanlib/SConstruct` - ClanLib build script

Key settings:
- **ClanLib**: Uses bundled version in `external/clanlib/` (not system-installed)
- **Compiler flags**: Includes `-fPIC` for position-independent code
- **Ruby**: Automatically detects Ruby installation

### Cleaning

Clean compiled objects:
```bash
scons -c
```

This removes all `.o` files and libraries but not the final binaries.

### Parallel Builds

The build system automatically detects your CPU count and uses parallel compilation by default for faster builds. You can also explicitly control parallelism:

```bash
scons -j4   # Use 4 parallel jobs
scons -j    # Use all available CPUs
scons -j1   # Serial build (one job at a time)
```

For multi-core systems, parallel builds are significantly faster:
- **2 cores**: ~2x faster
- **4 cores**: ~3-4x faster  
- **8+ cores**: ~6-8x faster (diminishing returns)

### Keeping Source Clean

Build artifacts (`.o`, `.a`, `.so` files) are created alongside source files. To keep the source tree clean for version control, add common build outputs to your `.gitignore` (already done in the repo):

```
*.o
*.a
*.so
.sconsign.dblite
```

All build artifacts are already ignored by git. You can safely rebuild without cluttering your working directory in version control.

## Troubleshooting

### Build Fails with "pkg-config not found"
This is expected. The bundled ClanLib is used instead of system packages.

### "cannot find -lclanXXX" errors
Ensure ClanLib is built first:
```bash
cd external/clanlib
scons
cd ../..
scons -c
scons
```

### Ruby header conflicts
The build properly orders includes to avoid macro conflicts between Ruby and Boost headers. If you see compilation errors mentioning `snprintf` in Ruby headers, ensure Ruby headers are included first.

### OpenGL errors at runtime
Ensure you have:
- OpenGL development libraries installed
- Working graphics drivers
- Set `DISPLAY` environment variable if running remotely

## Architecture Notes

### ClanLib Patches

The bundled ClanLib has been patched for modern C++ (C++17) compatibility:

1. **signal_v3.h**: Fixed member access in copy constructor (line 93)
2. **datafile_inputprovider.h**: Removed explicit template parameters from `std::make_pair`
3. **inputsource_file.cpp**: Added `#include <unistd.h>` for `getcwd()`
4. **cl_assert.h**: Added `#include <cstddef>` for `NULL`
5. **png_provider_generic.cpp**: Updated `setjmp` call from `png_ptr->jmpbuf` to `png_jmpbuf(png_ptr)` for libpng 1.4+ compatibility

These patches are permanent modifications to the bundled code and are required for building with modern C++ toolchains.

### Ruby Bindings

Ruby bindings are generated using SWIG and provide:
- Flexlay core functionality to Ruby scripts
- NetPanzer editor scripting interface
- Game-specific editor logic

The bindings are compiled as shared objects (`.so` files) and loaded at runtime.

## Development

### Parallel Builds

The build system is configured to use all available CPU cores by default for faster compilation:

```bash
scons           # Automatically uses all CPU cores
scons -j8       # Explicitly use 8 parallel jobs
scons -j1       # Serial build (one job at a time)
```

### Incremental Builds
SCons caches build state in `.sconsign.dblite`. To force a full rebuild:

```bash
rm .sconsign.dblite
scons
```

### Verbose Output
For detailed build information:

```bash
scons --debug=all
```

## Running Editors

After building successfully:

```bash
# NetPanzer editor
cd netpanzer && ruby netpanzer-editor

# SuperTux editor  
cd supertux && ruby supertux-editor

# Paint application
cd paint && ruby paint
```

You may need to adjust game data paths in the editor scripts.

## Testing

No automated tests are currently configured. Manual testing by running the editors is recommended.

## Contributing

When making changes that affect the build:
1. Test with a clean build: `scons -c && scons`
2. Verify all editors still build and run
3. Document any new build dependencies
4. Update this BUILD.md if relevant

## See Also

- INSTALL - Installation and basic build instructions
- SConstruct - Main build configuration
- external/clanlib/SConstruct - ClanLib build configuration
