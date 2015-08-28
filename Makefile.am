## Process this file with automake to produce Makefile.in

SUBDIRS = src data po

mimedir = $(datadir)/mime/packages
mime_DATA = src/mime-type/spacefm-mime.xml

bin_SCRIPTS = spacefm-installer

EXTRA_DIST = \
	autogen.sh \
	$(mime_DATA) \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in
	
if NO_PIXMAPS
all-local:
	@echo
	@echo "IMPORTANT: After installation, run:"
	@echo "           sudo gtk-update-icon-cache -q -t -f $(datadir)/icons/hicolor"
	@echo "           sudo gtk-update-icon-cache -q -t -f $(datadir)/icons/Faenza"
	@echo
endif
