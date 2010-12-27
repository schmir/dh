pkgname=dh
pkgver=4
pkgrel=1
pkgdesc="daemon helper"
arch=('i686' 'x86_64')
url="https://github.com/schmir/dh"
license=('BSD')
makedepends=('gcc')
depends=()
source=("dh.c" "Makefile")

build() {
  make || return 1
  make DESTDIR=$pkgdir install || return 1
}

md5sums=('efb42848b4c9114639df1185498ea1c1'
	 'fed8fe2c6adfcf29c90385655958cacb')
