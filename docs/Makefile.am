if USE_DOXYGEN

doxyfile.stamp: Doxyfile
	$(DOXYGEN) $<
	echo Timestamp > $@

CLEANFILES = doxyfile.stamp

all-local: doxyfile.stamp

clean-local:
	-rm -rf html/

endif
