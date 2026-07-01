CC ?= cc
USER_CPPFLAGS := $(CPPFLAGS)
USER_CFLAGS := $(CFLAGS)
USER_LDFLAGS := $(LDFLAGS)

REQUIRED_CPPFLAGS := -D_GNU_SOURCE
REQUIRED_CFLAGS := -std=c11 -Wall -Wextra -Werror -O2 -fPIC
LDFLAGS_SO ?= -shared -ldl -pthread

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
LIBDIR ?= $(PREFIX)/lib/runpod-shim
DOCDIR ?= $(PREFIX)/share/doc/runpod-shim

LIB := librunpod-shim.so
SRC := $(shell find src -name '*.c' 2>/dev/null)
OBJ := $(SRC:.c=.o)

.PHONY: all clean test install uninstall package-deb package-deb-compatible fixtures

all: $(LIB)

$(LIB): $(OBJ)
	$(CC) -o $@ $(OBJ) $(USER_LDFLAGS) $(LDFLAGS_SO)

src/%.o: src/%.c
	$(CC) $(REQUIRED_CPPFLAGS) $(USER_CPPFLAGS) $(REQUIRED_CFLAGS) $(USER_CFLAGS) -MMD -MP -c $< -o $@

-include $(OBJ:.o=.d)

tests/fixtures/%.exe: tests/fixtures/%.c $(filter-out src/preload.o,$(OBJ))
	$(CC) $(REQUIRED_CPPFLAGS) $(USER_CPPFLAGS) $(REQUIRED_CFLAGS) $(USER_CFLAGS) -I. $< $(filter-out src/preload.o,$(OBJ)) -ldl -pthread -o $@

fixtures: tests/fixtures/cgroup_probe.exe tests/fixtures/memory_probe.exe tests/fixtures/read_meminfo.exe

test: all fixtures
	@set -eu; for t in tests/test_*.sh; do echo "==> $$t"; sh "$$t"; done

install: $(LIB)
	install -d "$(DESTDIR)$(BINDIR)" "$(DESTDIR)$(LIBDIR)" "$(DESTDIR)$(DOCDIR)"
	install -m 0755 "$(LIB)" "$(DESTDIR)$(LIBDIR)/$(LIB)"
	install -m 0755 tools/runpod-shim "$(DESTDIR)$(BINDIR)/runpod-shim"
	install -m 0755 tools/runpod-shim-probe "$(DESTDIR)$(BINDIR)/runpod-shim-probe"
	sed 's|^default_lib=.*|default_lib="$(LIBDIR)/$(LIB)"|' tools/runpod-shim-install-global > "$(DESTDIR)$(BINDIR)/runpod-shim-install-global"
	chmod 0755 "$(DESTDIR)$(BINDIR)/runpod-shim-install-global"
	install -m 0644 README.md "$(DESTDIR)$(DOCDIR)/README.md"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/runpod-shim"
	rm -f "$(DESTDIR)$(BINDIR)/runpod-shim-probe"
	rm -f "$(DESTDIR)$(BINDIR)/runpod-shim-install-global"
	rm -f "$(DESTDIR)$(LIBDIR)/$(LIB)"

package-deb: all
	sh packaging/build-deb.sh

package-deb-compatible:
	sh packaging/build-compatible-deb-docker.sh

clean:
	rm -f $(LIB) $(OBJ) $(OBJ:.o=.d)
	rm -rf build tmp tests/fixtures/*.exe tests/fixtures/*.out
