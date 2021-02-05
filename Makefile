sinclude .crosscompile
sinclude version

export VERSION
export RELEASE

ifndef ARCH
ARCH = i386
endif

ifndef DEBIAN_VERSION
DEBIAN_VERSION = 9
endif

export DEB_HOST_ARCH=$(ARCH)
export DEBIAN_VERSION

SRC=coredump.c
BIN=coredump

PWD=$(shell pwd)

default: install

bin:
	$(CROSS_COMPILE)gcc -Wall $(SRC) -o $(BIN)

install: bin
	for f in install_* ; \
	do if [ -L "$$f" ] ;then \
		cp -fv $(BIN) $$f/opt/bin; \
	fi; done
re:
	make clean
	make

clean:
	rm -f *.o *~ $(BIN)

debu:
	fakeroot debian/rules binary

debc:
	fakeroot debian/rules clean

rpm:
	rpmbuild -bb \
		--define "%version_major $(VERSION)" \
		--define "%version_minor $(RELEASE)" \
		--define "%_rpmdir ./" ./redhat/coredump.spec
