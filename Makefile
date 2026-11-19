APXS ?= $(shell command -v apxs || command -v apxs2)

.PHONY: all install enable-debian clean test

all: .libs/mod_ai_agent.so

.libs/mod_ai_agent.so: mod_ai_agent.c
	$(APXS) -c -Wc,-Wall mod_ai_agent.c -lcurl

# Copies the module into Apache's module directory and adds a LoadModule line
# (on RHEL/Fedora/macOS/source builds).
install: all
	$(APXS) -i -a -n ai_agent .libs/mod_ai_agent.so

# Debian/Ubuntu: install, register with a2enmod and reload.
enable-debian: all
	$(APXS) -i -n ai_agent .libs/mod_ai_agent.so
	echo "LoadModule ai_agent_module /usr/lib/apache2/modules/mod_ai_agent.so" > /etc/apache2/mods-available/ai_agent.load
	cp ai_agent.conf /etc/apache2/mods-available/ai_agent.conf
	a2enmod ai_agent
	apache2ctl configtest && systemctl reload apache2

test:
	./tests/smoke.sh $(URL)

clean:
	rm -rf .libs *.la *.lo *.slo *.o
