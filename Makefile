# this Makefile is not supposed to be edited, but our use for it is somewhat different from google's use for it
# we only care about compiling art and it's dependencies, while the build system is meant to compile the whole AOSP

# we use four underscores for things only used by this Makefile,
# and two underscores for configuration variables we added and which
# we are defining here
# no underscores are used by preexising configuration variables which
# we set here as well

____TOPDIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# override this as appropriate for packaging
____PREFIX ?= /usr/local# sane default
____BINDIR ?= bin# probably fine for most distros
____LIBDIR ?= $(shell (ld --verbose | grep SEARCH_DIR | grep -oE "/usr/lib([^\"]*)" || echo /usr/lib) | head -n 1 | cut -c 6-)
____INCLUDEDIR ?= include# probably fine for most distros
# dx.jar and it's wrapper script should arguably be in a separate package
# TODO: split into a separate repo and use it from system?
____PREFIX_DX ?= $(____PREFIX)

____INSTALL_BINDIR = $(____PREFIX)/$(____BINDIR)
____INSTALL_LIBDIR = $(____PREFIX)/$(____LIBDIR)
____INSTALL_INCLUDEDIR = $(____PREFIX)/$(____INCLUDEDIR)

____INSTALL_BINDIR_DX = $(____PREFIX_DX)/$(____BINDIR)

# The Android 10 boot class path's jars (core-oj.jar, core-libart.jar,
# framework.jar and the rest), from AOSP's build of android-security-10.0.0_r75:
# Mead's Scripts/aosp/build-android-10-java.sh leaves them in out/mead-java,
# with the ICU and time zone data the core library reads beside them, in
# etc/icu and etc/tz, and the CA certificates in etc/security/cacerts.
____ANDROID_JAVA_DIR ?= $(____TOPDIR)/out/android-java
# Where that data goes: what ANDROID_RUNTIME_ROOT and ANDROID_TZDATA_ROOT
# name when the runtime starts, and ANDROID_ROOT, Android's /system: Conscrypt's
# CA certificates in etc/security/cacerts, the framework's resources in
# framework/framework-res.apk, and build.prop.
____INSTALL_RUNTIME_ROOT = $(____PREFIX)/share/art
# Android's name for this machine's CPU, for build.prop's ABI list.
____ANDROID_ABI := $(if $(filter x86_64,$(shell uname -m)),x86_64,arm64-v8a)
____INSTALL_LIBDIR_DX = $(____PREFIX_DX)/$(____LIBDIR)

____dalvikvm_bin_32 := $(____TOPDIR)/out/host/linux-x86/bin/dalvikvm
____dalvikvm_bin_64 := $(____TOPDIR)/out/host/linux-x86/bin/dalvikvm64

# while this *looks* portable, do note that ____LIBDIR is still distro specific
__RPATH_BIN := \$${ORIGIN}/../$(____LIBDIR)/art/:\$${ORIGIN}/../$(____LIBDIR)/java/dex/art/natives
# libraries look beside themselves and in the other of the two directories
# the install puts them in: art/ and java/dex/art/natives/
__RPATH_LIB := \$${ORIGIN}:\$${ORIGIN}/../../../../art:\$${ORIGIN}/../java/dex/art/natives

__VERSION ?= 0.0.0

# Jack compiler was google's first attempt at a solution for desugaring Java 8 and newer bytecode into bytecode compatible with older dex file formats.
# Thankfully, google realized that this was a bad idea, and in a typical google fashion, axed the project.
# They experimented with other approaches, but currently (2022) the way to do this is to use d8/r8 instead of dx;
# d8/r8 is technically supposed to build standalone, unlike most of google's stuff, but it requires depot tools,
# and would be a PITA to convert to something sane that can be packaged
# Thanfully we don't seem to need any desugaring
ANDROID_COMPILE_WITH_JACK := false
# build only for one architecture at a time, like any sane build system does
# note that this is localy overwritten in some places, but in all the places where they seemed to do something the overrides were removed
LOCAL_MULTILIB := none
# only the gcc build was cleaned up and tested to work; if you want to build with clang,
# rather than just setting this to false you should probably make the gcc path generic to both
# compilers (bonus points for pruning dead Makefile code)
WITHOUT_CLANG := true
# no idea what this is, but it's not present in our somewhat lean source tree and it's clearly not needed
WITHOUT_LIBCOMPILER_RT := true

include system/core/libdl_bio/system_dl_bio.mk
# the fallback libdl_bio, when bionic_translation is not installed
ifeq ($(ART_SYSTEM_DL_BIO),)
____dl_bio_fallback := libdl_bio
endif

default: ____art_all

____art_all: $(____dl_bio_fallback) dalvikvm art dex2oat libnativehelper libandroidio libjavacore libopenjdk libopenjdkjvm libjavacrypto libandroidfw libbinder libui libnativewindow libgui libhwui libinput libandroid_runtime art-standalone.pc

art-standalone.pc: art-standalone.pc.in
	sed -e 's|@prefix@|$(____PREFIX)|g' \
	    -e 's|@libdir@|$(____LIBDIR)|g' \
	    -e 's|@includedir@|$(____INCLUDEDIR)|g' \
	    -e 's|@version@|$(__VERSION)|g' \
	    $< > $@

install:
# this is a JNI library, so it belongs next to it's java counterpart
# TODO: libwolfssljni.so from distro wolfssl package should presumably work, so assuming it's packaged for use with OpenJDK, we could just symlink it here
	install -Dt $(____INSTALL_LIBDIR)/java/dex/art/natives/ $(____TOPDIR)/out/host/linux-x86/lib64/libjavacore.so \
	                                                        $(____TOPDIR)/out/host/linux-x86/lib64/libnativehelper.so \
	                                                        $(____TOPDIR)/out/host/linux-x86/lib64/libopenjdk.so \
	                                                        $(____TOPDIR)/out/host/linux-x86/lib64/libopenjdkjvm.so \
	                                                        $(____TOPDIR)/out/host/linux-x86/lib64/libandroidio.so \
	                                                        $(____TOPDIR)/out/host/linux-x86/lib64/libjavacrypto.so \
	                                                        $(____TOPDIR)/out/host/linux-x86/lib64/libandroid_runtime.so \
	                                                        $(____TOPDIR)/out/host/linux-x86/lib64/libbinder.so
#
# these are mostly internal libs which there's no point in versioning because they don't have any kind of stable ABI - so we just shove them in art/ namespace
# even the libunwind is actually a fork of libunwind 1.1 with added stuff that will likely never be upstreamed, especially since google stopped using libunwind
	install -Dt $(____INSTALL_LIBDIR)/art/ $(____TOPDIR)/out/host/linux-x86/lib64/libandroidfw.so \
	                                       $(____TOPDIR)/out/host/linux-x86/lib64/libmead_android10_compat.so \
	                                       $(____TOPDIR)/out/host/linux-x86/lib64/libgui.so \
	                                       $(____TOPDIR)/out/host/linux-x86/lib64/libhwui.so \
	                                       $(____TOPDIR)/out/host/linux-x86/lib64/libinput.so \
	                                       $(____TOPDIR)/out/host/linux-x86/lib64/libnativewindow.so \
	                                       $(____TOPDIR)/out/host/linux-x86/lib64/libui.so \
	                                       $(____TOPDIR)/out/host/linux-x86/lib64/libart.so \
	                                       $(____TOPDIR)/out/host/linux-x86/lib64/libart-compiler.so \
	                                       $(____TOPDIR)/out/host/linux-x86/lib64/libart-dexlayout.so \
	                                       $(____TOPDIR)/out/host/linux-x86/lib64/libartbase.so \
	                                       $(____TOPDIR)/out/host/linux-x86/lib64/libartpalette.so \
	                                       $(____TOPDIR)/out/host/linux-x86/lib64/libbacktrace.so \
	                                       $(____TOPDIR)/out/host/linux-x86/lib64/libbase.so \
	                                       $(____TOPDIR)/out/host/linux-x86/lib64/libcutils.so \
	                                       $(____TOPDIR)/out/host/linux-x86/lib64/libdexfile.so \
	                                       $(____TOPDIR)/out/host/linux-x86/lib64/liblog.so \
	                                       $(____TOPDIR)/out/host/linux-x86/lib64/libnativebridge.so \
	                                       $(____TOPDIR)/out/host/linux-x86/lib64/libprofile.so \
	                                       $(____TOPDIR)/out/host/linux-x86/lib64/libsigchain.so \
	                                       $(____TOPDIR)/out/host/linux-x86/lib64/libunwind.so \
	                                       $(____TOPDIR)/out/host/linux-x86/lib64/libutils.so \
	                                       $(____TOPDIR)/out/host/linux-x86/lib64/libziparchive.so
ifeq ($(ART_SYSTEM_DL_BIO),)
	install -Dt $(____INSTALL_LIBDIR)/art/ $(____TOPDIR)/out/host/linux-x86/lib64/libdl_bio.so
endif
#
# note: `test -f` should install the 32bit binary when it's not a symlink to the 64 bit one
	test -f $(____dalvikvm_bin_32) && install -Dt $(____INSTALL_BINDIR) $(____dalvikvm_bin_32) || true
	test -f $(____dalvikvm_bin_64) && install -Dt $(____INSTALL_BINDIR) $(____dalvikvm_bin_64) || true
	install -Dt $(____INSTALL_BINDIR) $(____TOPDIR)/out/host/linux-x86/bin/dex2oat
# packaging note: this should arguably be it's own package (together with out/host/linux-x86/framework/dx.jar)
	install -Dt $(____INSTALL_BINDIR_DX) $(____TOPDIR)/out/host/linux-x86/bin/dx
#
# the dexed libraries: Android 10's boot class path, from AOSP's build
# packaging note: these are architecture-independent
	install -m644 -Dt $(____INSTALL_LIBDIR)/java/dex/art/ $(wildcard $(____ANDROID_JAVA_DIR)/*.jar)
	install -m644 -Dt $(____INSTALL_RUNTIME_ROOT)/etc/icu/ $(wildcard $(____ANDROID_JAVA_DIR)/etc/icu/*)
	install -m644 -Dt $(____INSTALL_RUNTIME_ROOT)/etc/tz/ $(wildcard $(____ANDROID_JAVA_DIR)/etc/tz/*)
	install -m644 -Dt $(____INSTALL_RUNTIME_ROOT)/etc/security/cacerts/ $(wildcard $(____ANDROID_JAVA_DIR)/etc/security/cacerts/*)
# the system fonts and their list, Android's /system/fonts and
# /system/etc/fonts.xml
	install -m644 -Dt $(____INSTALL_RUNTIME_ROOT)/etc/ $(wildcard $(____ANDROID_JAVA_DIR)/etc/fonts.xml)
	install -m644 -Dt $(____INSTALL_RUNTIME_ROOT)/fonts/ $(wildcard $(____ANDROID_JAVA_DIR)/fonts/*)
# the key character maps, Android's /system/usr/keychars
	install -m644 -Dt $(____INSTALL_RUNTIME_ROOT)/usr/keychars/ $(wildcard $(____ANDROID_JAVA_DIR)/usr/keychars/*)
# the framework's resources, where the framework looks for them: /system,
# which is ANDROID_ROOT here
	install -m644 -Dt $(____INSTALL_RUNTIME_ROOT)/framework/ $(wildcard $(____ANDROID_JAVA_DIR)/framework-res.apk)
# what init loads from /system/build.prop on Android, for this machine's ABI
	sed -e 's|@ABI@|$(____ANDROID_ABI)|g' $(____TOPDIR)/frameworks/mead/build.prop.in > $(____INSTALL_RUNTIME_ROOT)/build.prop
#
# packaging note: this should arguably be it's own package (together with out/host/linux-x86/bin/dx)
	install -Dt $(____INSTALL_LIBDIR_DX)/java/ $(____TOPDIR)/out/host/linux-x86/framework/dx.jar

	# install pkg-config file
	mkdir -p $(____INSTALL_LIBDIR)/pkgconfig
	install -m644 art-standalone.pc $(____INSTALL_LIBDIR)/pkgconfig/

# install, without the debug info: it is most of the libraries' size (libart.so
# alone has about 250 MB of it). Their symbols stay, for ART's backtraces.
____STRIP ?= strip
install-strip: install
	find $(____INSTALL_LIBDIR)/art $(____INSTALL_LIBDIR)/java/dex/art/natives -name "*.so" -exec $(____STRIP) --strip-debug {} +
	$(____STRIP) --strip-debug $(wildcard $(____INSTALL_BINDIR)/dalvikvm $(____INSTALL_BINDIR)/dalvikvm64 $(____INSTALL_BINDIR)/dex2oat)

# TODO: figure out sharing dependencies and have this in a separate repo
install_adbd:
	install -Dt $(____INSTALL_BINDIR) $(____TOPDIR)/out/host/linux-x86/bin/adbd

### DO NOT EDIT THIS FILE ###
include build/core/main.mk
### DO NOT EDIT THIS FILE ###
