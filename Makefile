APXS ?= $(shell command -v apxs || command -v apxs2)

.PHONY: all install enable-debian clean test

all: .libs/mod_http_b.so

.libs/mod_http_b.so: mod_http_b.c
	$(APXS) -c -Wc,-Wall mod_http_b.c -lcurl

# Copies the module into Apache's module directory and adds a LoadModule line
# (on RHEL/Fedora/macOS/source builds).
install: all
	$(APXS) -i -a -n http_b .libs/mod_http_b.so

# Debian/Ubuntu: install, register with a2enmod and reload.
enable-debian: all
	$(APXS) -i -n http_b .libs/mod_http_b.so
	echo "LoadModule http_b_module /usr/lib/apache2/modules/mod_http_b.so" > /etc/apache2/mods-available/http_b.load
	cp http_b.conf /etc/apache2/mods-available/http_b.conf
	a2enmod http_b
	apache2ctl configtest && systemctl reload apache2

test:
	./tests/smoke.sh $(URL)

clean:
	rm -rf .libs *.la *.lo *.slo *.o
