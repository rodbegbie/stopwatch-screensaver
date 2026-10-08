# Source this: `source tools/env.sh`. Keeps PlatformIO and Python tooling
# inside this repo so cleanup is `rm -rf .venv .platformio .pio`.
_xs_root="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")/.." && pwd)"
export PLATFORMIO_CORE_DIR="$_xs_root/.platformio"
export PATH="$_xs_root/.venv/bin:$PATH"
unset _xs_root
