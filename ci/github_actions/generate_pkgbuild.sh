echo -n "pkgname=aria
pkgver=$GITHUB_REF_NAME
pkgrel=1
pkgdesc=\"An experimental low-level programming language built to improve on C.\"
arch=('x86_64')
url=\"https://shkhuz.github.io/aria\"
license=('GPL3')
depends=('llvm')
makedepends=()
provides=('ariac')
source=(\"\${pkgname}::git+https://github.com/shkhuz/aria.git#tag=\$pkgver\")
md5sums=('SKIP')

build() {
	cd "\$srcdir/\${pkgname}"
	make build/ariac
}

package() {
	cd "\$srcdir/\${pkgname}"
	make DESTDIR="\$pkgdir/" install
}" > PKGBUILD
