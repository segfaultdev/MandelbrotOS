include ../../config.mk

SUBDIRS = $(shell ls -d */)

PROG_DIRECTORY_GUARD = mkdir -p $(PROG_BUILD_DIRECTORY)

all: prog

prog:
	for dir in $(SUBDIRS) ; do \
		$(PROG_DIRECTORY_GUARD) ; \
		make -C $$dir ; \
	done
