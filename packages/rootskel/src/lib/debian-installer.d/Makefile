dir = lib/debian-installer.d

DEB_HOST_ARCH_CPU   := $(shell dpkg-architecture -qDEB_HOST_ARCH_CPU 2>/dev/null)
DEB_HOST_ARCH_OS    := $(shell dpkg-architecture -qDEB_HOST_ARCH_OS 2>/dev/null)

# Take account of old dpkg-architecture output.
ifeq ($(DEB_HOST_ARCH_CPU),)
  DEB_HOST_ARCH_CPU   := $(shell dpkg-architecture -qDEB_HOST_GNU_CPU)
  ifeq ($(DEB_HOST_ARCH_CPU),x86_64)
    DEB_HOST_ARCH_CPU   := amd64
  endif
endif
ifeq ($(DEB_HOST_ARCH_OS),)
  DEB_HOST_ARCH_OS    := $(shell dpkg-architecture -qDEB_HOST_GNU_SYSTEM)
endif

files += \
	S30term \
	S60frontend \
	S65theme \
	S70menu \
	S72menu-exit

ifeq ($(DEB_HOST_ARCH_CPU),sparc)
  files += \
	S34framebuffer-linux-sparc
endif


ifeq ($(DEB_HOST_ARCH_OS),linux)
  FRAMEBUFFER = 1
  ifeq ($(DEB_HOST_ARCH_CPU),s390)
    FRAMEBUFFER = 0
  endif
  ifeq ($(DEB_HOST_ARCH_CPU),s390x)
    FRAMEBUFFER = 0
  endif

  ifneq ($(FRAMEBUFFER),0)
    files += \
	S35framebuffer-linux
  endif

  files += \
	S20speakup \
	S40term-linux
endif
ifeq ($(DEB_HOST_ARCH_OS),kfreebsd)
  files += \
  	S40term-kfreebsd
endif
ifeq ($(DEB_HOST_ARCH_OS),hurd)
  files += \
  	S40term-hurd
endif

include ../../../Makefile.inc
