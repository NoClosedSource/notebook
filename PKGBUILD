pkgname=notebook
pkgver=1.0
pkgrel=1
pkgdesc="A simple, lightweight text editor with a bunch of basic features "
arch=('x86_64')
url="https://github.com/NoClosedSource/notebook/"
license=('GPL3')
depends=('gtk3')
makedepends=('gcc' 'make' 'pkgconf')
source=("notebook-$pkgver.tar.gz")
md5sums=('SKIP')  # Replace with actual checksum later

build() {
    gcc -Wall -o notebook main.cpp `pkg-config --cflags --libs gtk+-3.0`
}

package() {
    install -Dm755 notebook "$pkgdir/usr/bin/notebook"
    install -Dm644 notebook.png "$pkgdir/usr/share/icons/hicolor/128x128/apps/notebook.png"
    install -Dm644 notebook.desktop "$pkgdir/usr/share/applications/notebook.desktop"
}
