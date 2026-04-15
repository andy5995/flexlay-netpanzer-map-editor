# Build Instructions

## Dependencies

**Ubuntu/Debian:**
```sh
sudo apt install build-essential meson ninja-build qtbase5-dev qtbase5-dev-tools pkg-config
```

**Fedora:**
```sh
sudo dnf install gcc-c++ meson ninja-build qt5-qtbase-devel pkg-config
```

**Arch:**
```sh
sudo pacman -S base-devel meson qt5-base
```

**macOS (Homebrew):**
```sh
brew install meson ninja qt@5
```

## Building

**Linux / macOS:**
```sh
meson setup build
ninja -C build
```

On macOS, if meson cannot find Qt5, add it to your PATH first:
```sh
export PATH="$(brew --prefix qt@5)/bin:$PATH"
```

**MSYS2 (MINGW64):**
```sh
meson setup build
ninja -C build
```

**MSVC (from a Developer Command Prompt):**
```sh
meson setup build --backend=ninja
ninja -C build
```

## Running

```sh
./build/netpanzer-editor
```

Use **File → Open** to open a `.npm` map file. The editor locates the tileset
relative to the map file automatically.

## Testing

```sh
meson test -C build --print-errorlogs
```

## AddressSanitizer

ASan/UBSan are **not** enabled by default. To opt in:

```sh
meson setup build -Db_sanitize=address,undefined
ninja -C build
```

Known third-party leaks from fontconfig/Pango/GTK3 are suppressed via `asan.supp`.
`meson test` picks up the suppressions automatically via `LSAN_OPTIONS`.

To run the editor with suppression active:

```sh
LSAN_OPTIONS=suppressions=$(pwd)/asan.supp ./build/netpanzer-editor
```

## AppImage via Docker

The easiest way to produce a self-contained AppImage is via the
`andy5995/linuxdeploy:v3-jammy` container (available on Docker Hub),
which has all build tools and linuxdeploy pre-installed.

### Setup

Copy `.env.example` to `.env` and set `HOSTUID`/`HOSTGID` to your own
user so the container's entrypoint runs the build as that user and output
files are not owned by root:

```sh
cp .env.example .env
# Edit .env and set HOSTUID and HOSTGID, e.g.:
#   HOSTUID=$(id -u)
#   HOSTGID=$(id -g)
```

Also set `VERSION` in `.env` to the desired AppImage version string.

### Building

```sh
docker compose run --rm build
```

The resulting AppImage is written to `out/`.

Launch the AppImage and use **File → Open** to open a `.npm` map file.

### Testing changes to `make-appimage.sh`

Use `docker-compose.dev.yml` to get an interactive shell inside the container:

```sh
docker compose -f docker-compose.dev.yml run --rm build
```

From there you can run `./make-appimage.sh` directly or step through it manually.
