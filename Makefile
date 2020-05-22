#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPro")
endif

#PLATFORMS := gba ds7 ds9 3ds9
PLATFORMS := gba

.PHONY: all install clean $(PLATFORMS)

all: $(PLATFORMS)

install: all
	@mkdir -p $(DESTDIR)$(DEVKITPRO)/isan

clean:
	@echo Cleaning...
	@rm -rf build lib

$(PLATFORMS): lib
	@$(MAKE) --no-print-directory -f isan.mk IS_PLATFORM=$@

lib:
	@mkdir -p $@
