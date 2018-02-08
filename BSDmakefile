JARG =
GMAKE = "gmake"
#When gmake is called from another make instance, -w is automatically added
#which causes extraneous messages about directory changes to be emitted.
#--no-print-directory silences these messages.
GARGS = "--no-print-directory"

.if "$(.MAKE.JOBS)" != ""
JARG = -j$(.MAKE.JOBS)
.endif

#by default bmake will cd into ./obj first
.OBJDIR: ./

.PHONY: FRC
$(.TARGETS): FRC
	$(GMAKE) $(GARGS) $(.TARGETS:S,.DONE,,) $(JARG)

.DONE .DEFAULT: .SILENT
	$(GMAKE) $(GARGS) $(.TARGETS:S,.DONE,,) $(JARG)

.ERROR: .SILENT
	if ! which $(GMAKE) > /dev/null; then \
		echo "GNU Make is required!"; \
	fi
