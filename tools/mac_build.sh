#!/usr/bin/env bash
# Gene PHP extension — macOS build helper (Homebrew / phpize)
set -Eeuo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GENE_REPO="${GENE_REPO:-$(cd "$SCRIPT_DIR/.." && pwd)}"
GENE_SRC="${GENE_SRC:-$GENE_REPO/src}"
PHP_BIN="${PHP_BIN:-php}"
PHPIZE_BIN="${PHPIZE_BIN:-phpize}"
PHP_CONFIG_BIN="${PHP_CONFIG_BIN:-php-config}"
MAKE_BIN="${MAKE_BIN:-make}"
INSTALL="${INSTALL:-0}"
RUN_TESTS="${RUN_TESTS:-0}"
CLEAN="${CLEAN:-0}"

usage() {
    cat <<'EOF'
Usage: tools/mac_build.sh [options]

Build Gene on macOS via phpize + configure + make.
Success = src/modules/gene.so exists and `php --ri gene` loads it.

Options:
  --install       Run `sudo make install` after build
  --test          Also run test/TestRunner.php (not required to confirm the build)
  --clean         phpize --clean before build
  --php PATH      PHP CLI binary (default: php)
  --help          Show this help

Environment:
  GENE_REPO       Repository root (default: parent of tools/)
  GENE_SRC        Extension source dir (default: $GENE_REPO/src)
  PHP_BIN         PHP CLI (default: php)
  PHPIZE_BIN      Matching phpize (default: phpize)
  PHP_CONFIG_BIN  Matching php-config (default: php-config)

Homebrew:
  brew install php@8.1 autoconf pkg-config
  export PATH="$(brew --prefix php@8.1)/bin:$PATH"
  tools/mac_build.sh
EOF
}

while (($#)); do
    case "$1" in
        --install) INSTALL=1; shift ;;
        --test) RUN_TESTS=1; shift ;;
        --no-test) RUN_TESTS=0; shift ;;
        --clean) CLEAN=1; shift ;;
        --php)
            PHP_BIN="${2:?--php requires a path}"
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 64
            ;;
    esac
done

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "tools/mac_build.sh is intended for macOS (Darwin). Use phpize on Linux or task_gene_build.bat on Windows." >&2
    exit 2
fi

if [[ ! -f "$GENE_SRC/config.m4" ]]; then
    echo "Invalid GENE_SRC (missing config.m4): $GENE_SRC" >&2
    exit 2
fi

if ! command -v autoconf >/dev/null 2>&1; then
    echo "Missing autoconf (needed by phpize). Install: brew install autoconf pkg-config" >&2
    exit 2
fi

if ! command -v "$PHP_BIN" >/dev/null 2>&1; then
    echo "Missing required command: $PHP_BIN" >&2
    echo "Install Xcode Command Line Tools and PHP (e.g. brew install php@8.1 autoconf)." >&2
    echo "Then: export PATH=\"\$(brew --prefix php@8.1)/bin:\$PATH\"" >&2
    exit 2
fi

PHP_BIN="$(command -v "$PHP_BIN")"
PHP_DIR="$(dirname "$PHP_BIN")"
if [[ -x "$PHP_DIR/phpize" ]]; then
    PHPIZE_BIN="$PHP_DIR/phpize"
fi
if [[ -x "$PHP_DIR/php-config" ]]; then
    PHP_CONFIG_BIN="$PHP_DIR/php-config"
fi

for cmd in "$PHPIZE_BIN" "$PHP_CONFIG_BIN" "$MAKE_BIN"; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "Missing required command: $cmd" >&2
        echo "Use --php with a PHP installation that includes matching phpize and php-config." >&2
        exit 2
    fi
done
PHPIZE_BIN="$(command -v "$PHPIZE_BIN")"
PHP_CONFIG_BIN="$(command -v "$PHP_CONFIG_BIN")"
MAKE_BIN="$(command -v "$MAKE_BIN")"

PHP_VERSION="$("$PHP_BIN" -r 'echo PHP_VERSION;')"
PHP_MAJOR="$("$PHP_BIN" -r 'echo PHP_MAJOR_VERSION;')"
if ((PHP_MAJOR < 8)); then
    echo "Gene requires PHP 8.0+. Found: $PHP_VERSION" >&2
    exit 2
fi

echo "[mac_build] PHP $PHP_VERSION ($("$PHP_BIN" -m | head -1))"
echo "[mac_build] PHPIZE=$PHPIZE_BIN"
echo "[mac_build] ARCH=$(uname -m)"

(
    cd "$GENE_SRC"
    if ((CLEAN)); then
        "$PHPIZE_BIN" --clean >/dev/null 2>&1 || true
    fi
    "$PHPIZE_BIN"
    CFLAGS="${CFLAGS:--O2 -g -fno-omit-frame-pointer}" \
        ./configure --enable-gene=shared --with-php-config="$PHP_CONFIG_BIN"
    CORES="$(sysctl -n hw.ncpu 2>/dev/null || echo 2)"
    "$MAKE_BIN" -j"$CORES"
)

GENE_SO="$GENE_SRC/modules/gene.so"
if [[ ! -f "$GENE_SO" ]]; then
    echo "Build failed: $GENE_SO not found" >&2
    exit 1
fi
echo "[mac_build] Built $GENE_SO"

if ((INSTALL)); then
    (cd "$GENE_SRC" && sudo "$MAKE_BIN" install)
    echo "[mac_build] Installed to $("$PHP_CONFIG_BIN" --extension-dir)"
fi

EXT_DIR="$("$PHP_CONFIG_BIN" --extension-dir)"
PHP_ARGS=(-n)
for ext in pdo pdo_sqlite curl openssl; do
    if [[ -f "$EXT_DIR/$ext.so" ]]; then
        PHP_ARGS+=(-d "extension=$EXT_DIR/$ext.so")
    fi
done
PHP_ARGS+=(-d "extension=$GENE_SO")

echo "[mac_build] Smoke check:"
"$PHP_BIN" "${PHP_ARGS[@]}" --ri gene | head -5

if ((RUN_TESTS)); then
    echo "[mac_build] Running TestRunner..."
    "$PHP_BIN" "${PHP_ARGS[@]}" "$GENE_REPO/test/TestRunner.php"
fi

echo "[mac_build] Done. Add to php.ini: extension=$(realpath "$GENE_SO" 2>/dev/null || echo "$GENE_SO")"
