JARG =
GMAKE = "gmake"
.if "$(.MAKE.JOBS)" != ""
JARG = -j$(.MAKE.JOBS)
.endif

#by default bmake will cd into ./obj first
.OBJDIR: ./

.DONE .DEFAULT: .SILENT
	$(GMAKE) $(.TARGETS:S,.DONE,,) $(JARG)

.ERROR: .SILENT
	if ! which $(GMAKE) > /dev/null; then \
		echo "GNU Make is required!"; \
	fi

