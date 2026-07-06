# Maintainer: Sergei Shamanov <your@email>
# AUR package name: hyprsunset-solar
# Build from local git checkout:  cd <repo-root> && makepkg -si
#
# This is a fork of hyprsunset with added geolocation-based solar tracking:
#   - latitude / longitude → automatic sunrise/sunset calculation
#   - gradual colour temperature animation (Planckian + Illuminant D blending)
#   - configurable solar elevation thresholds
#   - fully backwards-compatible with upstream profiles + IPC

pkgname=hyprsunset-solar
pkgver=0.4.0
pkgrel=1
pkgdesc="Hyprland blue-light filter with geolocation solar tracking (fork of hyprsunset)"
arch=('x86_64')
url="https://github.com/sshamanov/hyprsunset"
license=('BSD')
depends=(
    'wayland'
    'hyprutils>=0.2.3'
    'hyprlang'
    'wayland-protocols'
    'gcc-libs'
)
makedepends=(
    'cmake'
    'gcc'
    'hyprland-protocols>=0.4.0'
    'hyprwayland-scanner>=0.4.0'
    'systemd'
)
provides=("${pkgname}")
conflicts=('hyprsunset' 'hyprsunset-git')

# ── build from the local source tree (PKGBUILD lives in repo root) ──────────
# To build from a remote git repo instead, replace the two functions below with:

# source=("${pkgname}::git+https://github.com/sshamanov/hyprsunset.git")
# pkgver() { cd "${srcdir}/${pkgname}"; printf "%s.r%s.%s" \
#   "$(<VERSION)" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"; }
# Then delete the cp line in build().

build() {
    cmake -B "$startdir/build-pkg" \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/usr \
          "$startdir"
    cmake --build "$startdir/build-pkg"
}

check() {
    "$startdir/build-pkg/hyprsunset" --help >/dev/null
    "$startdir/build-pkg/hyprsunset" --version
}

package() {
    # DESTDIR is prepended to every install destination (even absolute
    # paths like the systemd unit dir from pkg-config), so everything
    # lands under $pkgdir.
    DESTDIR="$pkgdir" cmake --install "$startdir/build-pkg"

    install -Dm644 "$startdir/LICENSE" \
        "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
