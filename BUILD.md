# Build Instructions

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
    libx11-dev libxmu-dev libgl1-mesa-dev libglu1-mesa-dev \
    libpng-dev libjpeg-dev libxrandr-dev
```

**CentOS/RHEL:**
```bash
sudo yum groupinstall "Development Tools"
sudo yum install boost-devel ruby-devel ruby-devel scons swig \
    libX11-devel libXmu-devel mesa-libGL-devel mesa-libGLU-devel \
    libpng-devel libjpeg-turbo-devel libXrandr-devel
```

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
5. **Disabled PNG provider**: Modern libpng not compatible with old ClanLib code

These patches are permanent modifications to the bundled code and are required for building with modern C++ toolchains.

### Ruby Bindings

Ruby bindings are generated using SWIG and provide:
- Flexlay core functionality to Ruby scripts
- NetPanzer editor scripting interface
- Game-specific editor logic

The bindings are compiled as shared objects (`.so` files) and loaded at runtime.

## Development

### Incremental Builds
SCons caches build state in `.sconsign.dblite`. To force a full rebuild:

```bash
rm .sconsign.dblite
scons
```

### Parallel Builds
SCons supports parallel building (use with caution):

```bash
scons -j4  # Use 4 parallel jobs
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
