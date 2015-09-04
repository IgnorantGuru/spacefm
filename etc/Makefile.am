EXTRA_DIST = spacefm.conf

install-data-hook:
	test -d $(DESTDIR)/$(sysconfdir)/spacefm || mkdir -p -- \
				$(DESTDIR)/$(sysconfdir)/spacefm
	test -f $(DESTDIR)/$(sysconfdir)/spacefm/spacefm.conf || $(INSTALL_DATA) \
				$(srcdir)/spacefm.conf \
				$(DESTDIR)/$(sysconfdir)/spacefm/spacefm.conf

