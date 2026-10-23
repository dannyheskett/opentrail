# ---------------------------------------------------------------------------
# opentrail build
#
# The desktop game (Linux, Windows, macOS: one file each, both modes, with a
# chooser when the command line names neither), the two web builds (classic
# and modern, each one self-contained WebAssembly game) and the tests.
# Android and iOS (modern mode only) follow the family layout (openbounty).
# ---------------------------------------------------------------------------
# engine: the rules (the original BASIC's logic), no display
ENGINE_SRC  := $(wildcard src/engine/*.c)
# machine: the emulated Apple II core both modes run on (clock, keyboard, speaker, tunes)
MACHINE_SRC := $(wildcard src/machine/*.c)
# legacy: the pixel-identical Apple II screens and the extension's routines
LEGACY_SRC  := $(wildcard src/legacy/*.c)
# modern: the new-art screens
MODERN_SRC  := $(wildcard src/modern/*.c)
# host: raylib window, input, audio, entry point
HOST_SRC    := $(wildcard src/host/*.c)
HDRS        := $(wildcard src/*/*.h)
# everything but the host, for headless tools and tests
CORE_SRC    := $(ENGINE_SRC) $(MACHINE_SRC) $(LEGACY_SRC) $(MODERN_SRC)

# The version, as the family's (openbounty's): the release's N, which the
# release workflow passes as RELEASE_VERSION; else the latest release-N tag
# (0 before the first). build/version.h carries it (OT_VERSION), shown as
# v1.0.N at the bottom right of the opening screen and by --version.
RELEASE_VERSION ?= $(shell git tag --list 'release-*' 2>/dev/null | sed -n 's/^release-\([1-9][0-9]*\)$$/\1/p' | sort -n | tail -1 | grep . || echo 0)

CFLAGS_COMMON := -std=c99 -Wall -Wextra -Isrc/engine -Isrc/machine -Isrc/legacy -Isrc/modern -Isrc/host -Ibuild

# ---------------------------------------------------------------------------
# Linux desktop (raylib, static)
# ---------------------------------------------------------------------------
RAYLIB  := third_party/raylib-install
GAME_SRC := $(CORE_SRC) $(HOST_SRC) build/embed.c build/embed_art.c
# One self-contained file: the assets are compiled in (build/embed.c, the
# original's files; build/embed_art.c, modern mode's pictures), raylib
# is linked statically and trimmed to what the game uses
# (scripts/raylib_flags.sh), unused code is dropped and the binary stripped,
# leaving libc and libm. GLFW loads X11 or Wayland, GL and the audio libraries
# at run time, as every desktop has them.
LDFLAGS := -L$(RAYLIB)/lib -Wl,-Bstatic -lraylib -Wl,-Bdynamic -lm -lpthread -ldl -lrt
OUT     := build/opentrail
ASSETS  := $(wildcard assets/*.bin)
ART     := $(wildcard assets/modern/*.png)

# The recorder's vendored encoders (third_party/minih264, minimp4)
REC_INC := -Ithird_party/minih264 -Ithird_party/minimp4

$(OUT): $(GAME_SRC) $(HDRS) build/version.h | build
	gcc $(CFLAGS_COMMON) $(REC_INC) -O2 -ffunction-sections -fdata-sections -I$(RAYLIB)/include $(GAME_SRC) -o $@ \
	    $(LDFLAGS) -Wl,--gc-sections -s

# Written when the version changes only, so an unchanged one rebuilds nothing.
build/version.h: FORCE | build
	@printf '#ifndef OT_VERSION_H\n#define OT_VERSION_H\n#define OT_VERSION "%s"\n#endif\n' "$(RELEASE_VERSION)" > $@.tmp
	@if ! cmp -s $@.tmp $@ 2>/dev/null; then mv $@.tmp $@; else rm $@.tmp; fi
FORCE:

build/embed.c: tools/art.py $(ASSETS) | build
	python3 tools/art.py embed $@ $(ASSETS)

build/embed_art.c: tools/art.py $(ART) | build
	python3 tools/art.py embed --art $@ $(ART)

# The classic web build carries no modern pictures: an empty table
build/embed_art_none.c: tools/art.py | build
	python3 tools/art.py embed --art $@

build:
	mkdir -p build

# ---------------------------------------------------------------------------
# Windows (x64 + x86, cross-compiled with mingw-w64): one .exe each, fully
# static (raylib, libgcc, winpthread), assets compiled in. raylib first:
# scripts/build_raylib_windows.sh.
# ---------------------------------------------------------------------------
RAYLIB_WIN64 := third_party/raylib-install-win64
RAYLIB_WIN32 := third_party/raylib-install-win32
WIN_CFLAGS   := $(CFLAGS_COMMON) -O2 -ffunction-sections   # not -fdata-sections: PE then stores zeroed statics as data
WIN_LDFLAGS  := -Wl,-Bstatic -lraylib -lopengl32 -lgdi32 -lwinmm -lpthread -Wl,-Bdynamic \
                -mwindows -static -static-libgcc -Wl,--gc-sections -s
OUT_WIN64    := build/opentrail-x64.exe
OUT_WIN32    := build/opentrail-x86.exe

windows: $(OUT_WIN64) $(OUT_WIN32)

$(OUT_WIN64): $(GAME_SRC) $(HDRS) build/version.h | build
	x86_64-w64-mingw32-gcc $(WIN_CFLAGS) $(REC_INC) -I$(RAYLIB_WIN64)/include $(GAME_SRC) -o $@ -L$(RAYLIB_WIN64)/lib $(WIN_LDFLAGS)

$(OUT_WIN32): $(GAME_SRC) $(HDRS) build/version.h | build
	i686-w64-mingw32-gcc $(WIN_CFLAGS) $(REC_INC) -I$(RAYLIB_WIN32)/include $(GAME_SRC) -o $@ -L$(RAYLIB_WIN32)/lib $(WIN_LDFLAGS)

# ---------------------------------------------------------------------------
# macOS (universal arm64 + x86_64): needs a Mac with Xcode's command-line
# tools. raylib first: scripts/build_raylib_mac.sh.
# ---------------------------------------------------------------------------
RAYLIB_MAC  := third_party/raylib-install-mac
MAC_ARCHES  := -arch arm64 -arch x86_64
MAC_LDFLAGS := $(MAC_ARCHES) -L$(RAYLIB_MAC)/lib -lraylib -lpthread \
               -framework Cocoa -framework IOKit -framework CoreVideo -framework OpenGL \
               -Wl,-dead_strip
OUT_MAC     := build/opentrail-mac

mac: $(OUT_MAC)

$(OUT_MAC): $(GAME_SRC) $(HDRS) build/version.h | build
	clang $(CFLAGS_COMMON) $(REC_INC) -O2 $(MAC_ARCHES) -I$(RAYLIB_MAC)/include $(GAME_SRC) -o $@ $(MAC_LDFLAGS)
	strip -x $@

run: $(OUT)
	./$(OUT)


TEST_DIR := build/tests
TESTS    := $(TEST_DIR)/test_engine $(TEST_DIR)/test_sound

all: $(OUT) test

$(TEST_DIR):
	mkdir -p $@

$(TEST_DIR)/test_engine: tests/test_engine.c tests/script_io.h $(ENGINE_SRC) $(HDRS) | $(TEST_DIR)
	gcc $(CFLAGS_COMMON) -O0 -g -fsanitize=address,undefined tests/test_engine.c -o $@ -lm

$(TEST_DIR)/test_sound: tests/test_sound.c $(MACHINE_SRC) src/modern/music.c $(HDRS) | $(TEST_DIR)
	gcc $(CFLAGS_COMMON) -O1 -g -fsanitize=address,undefined $< -o $@ -lm

# test_play: whole games of random keys through the legacy screens under the
# sanitizers (build/play), each to its end without an error. test_harness:
# the harness plays both modes through its commands (tests/test_harness.py).
PLAY_GAMES := $(shell seq 1 48)

test: $(TESTS) build/play build/harness
	@for t in $(TESTS); do ./$$t || exit 1; done
	@for s in $(PLAY_GAMES); do build/play monkey:40000:$$s build/tests/play.bin 30 2>build/tests/play.log \
	    && ! grep -q "frame limit\|missing asset\|runtime error\|AddressSanitizer" build/tests/play.log \
	    || { cat build/tests/play.log; echo "FAIL: test_play seed $$s"; exit 1; }; done
	@echo "PASS: test_play: $(words $(PLAY_GAMES)) games of random keys"
	@$(MAKE) --no-print-directory ios-purity
	@python3 tools/art.py anchors --check
	@python3 tests/test_harness.py

clean:
	rm -rf build

# ---------------------------------------------------------------------------
# Web (WebAssembly, Emscripten): two games, each one mode with everything
# compiled into its .wasm (no data file beside it): build/web/classic/ and
# build/web/modern/, each index.html + opentrail.js + opentrail.wasm. The
# game owns its loop, so it runs under ASYNCIFY: raylib's web
# WindowShouldClose() sleeps a moment each frame, which hands the browser its
# turn, and setjmp/longjmp (game over from any depth, the window closing) work
# through it. The Top Ten and the graves are kept in the browser's IndexedDB
# (-lidbfs.js; main.c). A fixed heap, not ALLOW_MEMORY_GROWTH: a growable
# heap's resizable ArrayBuffers are refused by WebGL's texImage2D. The
# recorder is off on the web (recorder.c). Needs emcc on PATH (emsdk) and
# raylib for the web: scripts/build_raylib_web.sh.
# ---------------------------------------------------------------------------
RAYLIB_WEB  := third_party/raylib-install-web
WEB_HOST    := $(filter-out src/host/encode_h264.c src/host/encode_mux.c,$(HOST_SRC))
WEB_CFLAGS  := $(CFLAGS_COMMON) -O2 -DPLATFORM_WEB -I$(RAYLIB_WEB)/include
WEB_LDFLAGS := -O2 -L$(RAYLIB_WEB)/lib -lraylib -lidbfs.js -sUSE_GLFW=3 -sASYNCIFY \
               -sINITIAL_MEMORY=134217728 -sSTACK_SIZE=1048576 -sFORCE_FILESYSTEM \
               --shell-file web/shell.html
WEB_CLASSIC := build/web/classic/index.html
WEB_MODERN  := build/web/modern/index.html

web: $(WEB_CLASSIC) $(WEB_MODERN)
web-classic: $(WEB_CLASSIC)
web-modern: $(WEB_MODERN)

$(WEB_CLASSIC): $(CORE_SRC) $(WEB_HOST) $(HDRS) build/embed.c build/embed_art_none.c web/shell.html build/version.h
	@command -v emcc >/dev/null || { echo "make web: emcc not on PATH (source emsdk_env.sh)"; exit 1; }
	@mkdir -p $(@D)
	emcc $(WEB_CFLAGS) -DOT_WEB_MODE=0 $(CORE_SRC) $(WEB_HOST) build/embed.c build/embed_art_none.c \
	    -o $(@D)/opentrail.html $(WEB_LDFLAGS)
	sed 's/@@TITLE@@/The Oregon Trail/' $(@D)/opentrail.html > $@ && rm $(@D)/opentrail.html

$(WEB_MODERN): $(CORE_SRC) $(WEB_HOST) $(HDRS) build/embed.c build/embed_art.c web/shell.html build/version.h
	@command -v emcc >/dev/null || { echo "make web: emcc not on PATH (source emsdk_env.sh)"; exit 1; }
	@mkdir -p $(@D)
	emcc $(WEB_CFLAGS) -DOT_WEB_MODE=1 $(CORE_SRC) $(WEB_HOST) build/embed.c build/embed_art.c \
	    -o $(@D)/opentrail.html $(WEB_LDFLAGS)
	sed 's/@@TITLE@@/Open Trail/' $(@D)/opentrail.html > $@ && rm $(@D)/opentrail.html

# Browsers will not load .wasm from file://: serve them
web-serve: web
	@echo "classic: http://localhost:8080/classic/   modern: http://localhost:8080/modern/"
	@cd build/web && python3 -m http.server 8080

# ---------------------------------------------------------------------------
# Release archives (dist/), one per platform, each holding the single-file
# game (both modes) and the README; the web's, each game's three files.
# Named for RELEASE_VERSION (above): opentrail-build-N-*.
# ---------------------------------------------------------------------------
VERSION_SLUG    := build-$(RELEASE_VERSION)
DIST    := dist
STAGING := build/staging
PKG     := opentrail-$(VERSION_SLUG)

dist-linux: $(OUT)
	@rm -rf $(STAGING)/linux && mkdir -p $(STAGING)/linux/$(PKG) $(DIST)
	cp $(OUT) README.md LICENSE NOTICE $(STAGING)/linux/$(PKG)/
	tar -C $(STAGING)/linux -czf $(DIST)/$(PKG)-linux-x86_64.tar.gz $(PKG)

dist-windows: $(OUT_WIN64) $(OUT_WIN32)
	@rm -rf $(STAGING)/win && mkdir -p $(STAGING)/win/$(PKG)-x64 $(STAGING)/win/$(PKG)-x86 $(DIST)
	cp $(OUT_WIN64) $(STAGING)/win/$(PKG)-x64/opentrail.exe
	cp $(OUT_WIN32) $(STAGING)/win/$(PKG)-x86/opentrail.exe
	cp README.md LICENSE NOTICE $(STAGING)/win/$(PKG)-x64/
	cp README.md LICENSE NOTICE $(STAGING)/win/$(PKG)-x86/
	(cd $(STAGING)/win && zip -qr ../../../$(DIST)/$(PKG)-windows-x64.zip $(PKG)-x64 && \
	    zip -qr ../../../$(DIST)/$(PKG)-windows-x86.zip $(PKG)-x86)

# Ad-hoc signed: Apple Silicon runs nothing unsigned. Not notarized, so the
# first run needs right-click > Open.
dist-mac: $(OUT_MAC)
	@rm -rf $(STAGING)/mac && mkdir -p $(STAGING)/mac/$(PKG) $(DIST)
	cp $(OUT_MAC) $(STAGING)/mac/$(PKG)/opentrail
	codesign --force --sign - $(STAGING)/mac/$(PKG)/opentrail
	cp README.md LICENSE NOTICE $(STAGING)/mac/$(PKG)/
	(cd $(STAGING)/mac && zip -qr ../../../$(DIST)/$(PKG)-macos-universal.zip $(PKG))

# The web's: one zip per game, as the family's site reads them from the
# latest release (danheskett's hugo.yml, as openbounty's two games):
# <prefix>-build-N-web-wasm.zip, its one directory <prefix>-build-N-web/
# holding one .html, .js and .wasm (and the README, LICENSE and NOTICE): opentrail- for the
# modern game, opentrail-classic- for the classic one.
dist-web: web
	@rm -rf $(STAGING)/web && mkdir -p $(STAGING)/web $(DIST)
	for g in modern:opentrail classic:opentrail-classic; do \
	    mode=$${g%%:*}; pre=$${g#*:}; dir=$$pre-$(VERSION_SLUG)-web; \
	    mkdir -p $(STAGING)/web/$$dir && \
	    cp build/web/$$mode/index.html build/web/$$mode/opentrail.js build/web/$$mode/opentrail.wasm README.md LICENSE NOTICE \
	        $(STAGING)/web/$$dir/ && \
	    (cd $(STAGING)/web && zip -qr ../../../$(DIST)/$$pre-$(VERSION_SLUG)-web-wasm.zip $$dir) || exit 1; \
	done

# ---------------------------------------------------------------------------
# Android (raylib's NativeActivity, no Gradle; the family's, openbounty's):
# modern mode by touch, landscape, arm64-v8a.
#   android       build/opentrail.apk  (debug-signed, to sideload)
#   android-play  build/opentrail.aab  (Play's App Bundle, signed with PLAY_*)
# The toolchain comes from the make command line, never the environment:
#   make android ANDROID_NDK=<ndk root> ANDROID_SDK_ROOT=<sdk root>
# raylib first: scripts/build_raylib_android.sh --ndk <ndk root>.
# ---------------------------------------------------------------------------
ANDROID_GOALS := android android-play dist-android dist-android-play
ifneq ($(filter $(ANDROID_GOALS),$(MAKECMDGOALS)),)
ifneq ($(filter environment%,$(origin ANDROID_NDK) $(origin ANDROID_SDK_ROOT)),)
$(error ANDROID_NDK and ANDROID_SDK_ROOT come from the make command line, not the environment)
endif
ifeq ($(ANDROID_NDK),)
$(error pass ANDROID_NDK=<ndk root> on the make command line)
endif
ifeq ($(ANDROID_SDK_ROOT),)
$(error pass ANDROID_SDK_ROOT=<sdk root> on the make command line)
endif
endif
ANDROID_API          ?= 24
# The shipped ABI; x86_64 only for the CI emulator's smoke test.
ANDROID_ABI          ?= arm64-v8a
ANDROID_BUILD_TOOLS  ?= 36.0.0
ANDROID_PLATFORM_VER ?= 36
ANDROID_APP_NAME     := opentrail

# versionCode: the release's number (Play wants it to rise), at least 1.
ANDROID_VERSION_CODE ?= $(RELEASE_VERSION)
ifeq ($(ANDROID_VERSION_CODE),0)
ANDROID_VERSION_CODE := 1
endif
ANDROID_VERSION_NAME ?= 1.0.$(ANDROID_VERSION_CODE)

RAYLIB_ANDROID    := third_party/raylib-install-android/$(ANDROID_ABI)
ANDROID_TOOLCHAIN := $(ANDROID_NDK)/toolchains/llvm/prebuilt/linux-x86_64
ANDROID_TRIPLE_arm64-v8a   := aarch64-linux-android
ANDROID_TRIPLE_x86_64      := x86_64-linux-android
ANDROID_TRIPLE    := $(ANDROID_TRIPLE_$(ANDROID_ABI))
ANDROID_CC        := $(ANDROID_TOOLCHAIN)/bin/$(ANDROID_TRIPLE)$(ANDROID_API)-clang
NATIVE_APP_GLUE   := $(ANDROID_NDK)/sources/android/native_app_glue
ANDROID_SDK_BT    := $(ANDROID_SDK_ROOT)/build-tools/$(ANDROID_BUILD_TOOLS)
ANDROID_JAR       := $(ANDROID_SDK_ROOT)/platforms/android-$(ANDROID_PLATFORM_VER)/android.jar

# The game without the MP4 encoders (the recorder is a stub there), and both
# embedded tables (the original's files and modern mode's pictures).
ANDROID_SRC     := $(CORE_SRC) $(WEB_HOST) build/embed.c build/embed_art.c
ANDROID_CFLAGS  := $(CFLAGS_COMMON) -O2 -DPLATFORM_ANDROID -fPIC \
                   -I$(RAYLIB_ANDROID)/include -I$(NATIVE_APP_GLUE)
# --wrap=fopen: raylib's Android backend wraps fopen (libraylib.a calls
# __real_fopen). 16 KB pages: Play's requirement from Android 15.
ANDROID_LDFLAGS := -shared -L$(RAYLIB_ANDROID)/lib -lraylib -Wl,--wrap=fopen \
                   -Wl,-z,max-page-size=16384,-z,common-page-size=16384 \
                   -Wl,--gc-sections -s \
                   -llog -landroid -lEGL -lGLESv2 -lOpenSLES -lm -lc -ldl
ANDROID_OBJ_DIR := build/obj-android/$(ANDROID_ABI)
ANDROID_OBJ     := $(patsubst %.c,$(ANDROID_OBJ_DIR)/%.o,$(ANDROID_SRC)) $(ANDROID_OBJ_DIR)/native_app_glue.o
ANDROID_APK_DIR := build/android
ANDROID_LIB     := $(ANDROID_APK_DIR)/lib/$(ANDROID_ABI)/lib$(ANDROID_APP_NAME).so
ANDROID_APK     := build/$(ANDROID_APP_NAME).apk
ANDROID_KEYSTORE ?= build/debug.keystore
ANDROID_JAVA_SRC := android/java/com/danheskett/opentrail/OpenTrailActivity.java
ANDROID_DEX     := build/dex/classes.dex
ANDROID_RES     := $(wildcard android/res/*/*)
JAVAC           ?= javac
ZIP_EXTRACT     := python3 -m zipfile -e
ZIP_CREATE      := python3 -m zipfile -c

android: $(ANDROID_APK)

$(ANDROID_OBJ_DIR)/native_app_glue.o: $(NATIVE_APP_GLUE)/android_native_app_glue.c
	@mkdir -p $(dir $@)
	$(ANDROID_CC) $(ANDROID_CFLAGS) -Wno-unused-parameter -c $< -o $@

$(ANDROID_OBJ_DIR)/%.o: %.c build/version.h $(HDRS)
	@mkdir -p $(dir $@)
	$(ANDROID_CC) $(ANDROID_CFLAGS) -c $< -o $@

$(ANDROID_LIB): $(ANDROID_OBJ)
	@mkdir -p $(dir $@)
	$(ANDROID_CC) $(ANDROID_OBJ) -o $@ $(ANDROID_LDFLAGS)
	scripts/check_elf_align.sh $(ANDROID_TOOLCHAIN)/bin/llvm-readelf $@

# OpenTrailActivity.java against the platform jar, then dexed.
$(ANDROID_DEX): $(ANDROID_JAVA_SRC)
	@rm -rf build/java-classes && mkdir -p build/java-classes $(dir $@)
	$(JAVAC) -source 1.8 -target 1.8 -Xlint:-options -classpath $(ANDROID_JAR) -d build/java-classes $(ANDROID_JAVA_SRC)
	$(ANDROID_SDK_BT)/d8 --min-api $(ANDROID_API) --lib $(ANDROID_JAR) \
	    --output build/dex build/java-classes/com/danheskett/opentrail/*.class

# A throwaway debug key for the sideload APK; Play builds sign with PLAY_*.
$(ANDROID_KEYSTORE):
	@mkdir -p $(dir $@)
	keytool -genkeypair -keystore $@ -storepass android -keypass android \
	    -alias $(ANDROID_APP_NAME) -keyalg RSA -keysize 2048 -validity 10000 \
	    -dname "CN=OpenTrail, O=OpenTrail, C=US"

$(ANDROID_APK): $(ANDROID_LIB) $(ANDROID_DEX) $(ANDROID_KEYSTORE) android/AndroidManifest.xml $(ANDROID_RES)
	$(ANDROID_SDK_BT)/aapt package -f -M android/AndroidManifest.xml -S android/res -I $(ANDROID_JAR) \
	    --version-code $(ANDROID_VERSION_CODE) --version-name $(ANDROID_VERSION_NAME) \
	    -F build/$(ANDROID_APP_NAME).unaligned.apk
	(cd $(ANDROID_APK_DIR) && $(ANDROID_SDK_BT)/aapt add ../../build/$(ANDROID_APP_NAME).unaligned.apk \
	    lib/$(ANDROID_ABI)/lib$(ANDROID_APP_NAME).so)
	(cd build/dex && $(ANDROID_SDK_BT)/aapt add ../$(ANDROID_APP_NAME).unaligned.apk classes.dex)
	$(ANDROID_SDK_BT)/zipalign -f 4 build/$(ANDROID_APP_NAME).unaligned.apk build/$(ANDROID_APP_NAME).aligned.apk
	$(ANDROID_SDK_BT)/apksigner sign --ks $(ANDROID_KEYSTORE) --ks-pass pass:android --key-pass pass:android \
	    --out $@ build/$(ANDROID_APP_NAME).aligned.apk
	@rm -f build/$(ANDROID_APP_NAME).unaligned.apk build/$(ANDROID_APP_NAME).aligned.apk
	@echo "[android] built $@ (versionCode $(ANDROID_VERSION_CODE), versionName $(ANDROID_VERSION_NAME))"

# The App Bundle for Play: aapt2 (proto resources) and bundletool, signed with
# the upload key (Play App Signing re-signs what it delivers). PLAY_* default
# to the debug key, so the path can be tried locally; CI passes the real key.
ANDROID_AAB        := build/$(ANDROID_APP_NAME).aab
BUNDLETOOL_VERSION ?= 1.17.2
BUNDLETOOL         ?= build/bundletool.jar
BUNDLETOOL_SHA256  ?= 2d4ad908faea64047c1cc9cb747e6aa667c6ab192e09607bd16b67246a8cd6ae
PLAY_KEYSTORE   ?= $(ANDROID_KEYSTORE)
PLAY_KEY_ALIAS  ?= $(ANDROID_APP_NAME)
PLAY_STORE_PASS ?= android
PLAY_KEY_PASS   ?= android

android-play: $(ANDROID_AAB)

# Renamed into place only once its checksum matches.
$(BUNDLETOOL):
	@mkdir -p $(dir $@)
	curl -fsSL -o $@.tmp https://github.com/google/bundletool/releases/download/$(BUNDLETOOL_VERSION)/bundletool-all-$(BUNDLETOOL_VERSION).jar
	echo "$(BUNDLETOOL_SHA256)  $@.tmp" | sha256sum -c - || { rm -f $@.tmp; exit 1; }
	mv $@.tmp $@

# The passwords reach jarsigner through the environment, not the log.
$(ANDROID_AAB): export PLAY_STORE_PASS_ENV = $(PLAY_STORE_PASS)
$(ANDROID_AAB): export PLAY_KEY_PASS_ENV   = $(PLAY_KEY_PASS)
$(ANDROID_AAB): $(ANDROID_LIB) $(ANDROID_DEX) $(BUNDLETOOL) $(PLAY_KEYSTORE) android/AndroidManifest.xml $(ANDROID_RES)
	@rm -rf build/aab && mkdir -p build/aab/module/manifest build/aab/module/lib/$(ANDROID_ABI) build/aab/module/dex
	$(ANDROID_SDK_BT)/aapt2 compile --dir android/res -o build/aab/res.zip
	$(ANDROID_SDK_BT)/aapt2 link --proto-format -o build/aab/proto.apk -I $(ANDROID_JAR) \
	    --manifest android/AndroidManifest.xml -R build/aab/res.zip --auto-add-overlay \
	    --version-code $(ANDROID_VERSION_CODE) --version-name $(ANDROID_VERSION_NAME)
	$(ZIP_EXTRACT) build/aab/proto.apk build/aab/proto
	mv build/aab/proto/AndroidManifest.xml build/aab/module/manifest/AndroidManifest.xml
	mv build/aab/proto/resources.pb build/aab/module/resources.pb
	mv build/aab/proto/res build/aab/module/res
	cp $(ANDROID_LIB) build/aab/module/lib/$(ANDROID_ABI)/lib$(ANDROID_APP_NAME).so
	cp $(ANDROID_DEX) build/aab/module/dex/classes.dex
	cd build/aab/module && $(ZIP_CREATE) ../module.zip manifest resources.pb res lib dex
	java -jar $(BUNDLETOOL) build-bundle --modules=build/aab/module.zip --output=$@ --overwrite
	@jarsigner -keystore $(PLAY_KEYSTORE) -storepass:env PLAY_STORE_PASS_ENV -keypass:env PLAY_KEY_PASS_ENV \
	    -sigalg SHA256withRSA -digestalg SHA-256 $@ $(PLAY_KEY_ALIAS)
	@echo "[android] built $@ (versionCode $(ANDROID_VERSION_CODE), versionName $(ANDROID_VERSION_NAME))"

dist-android: android
	@mkdir -p $(DIST)
	cp $(ANDROID_APK) $(DIST)/$(PKG)-android-arm64.apk

dist-android-play: android-play
	@mkdir -p $(DIST)
	cp $(ANDROID_AAB) $(DIST)/$(PKG)-android.aab

# ---------------------------------------------------------------------------
# iOS (native: UIKit, Metal, AVAudioEngine; no raylib), the family's way: clang,
# the Info.plist and a zip, no Xcode project; needs macOS with Xcode, so CI's
# macOS runner builds it. Modern mode by touch, landscape. The game is the
# same C as everywhere (src/engine, machine, legacy, modern, host/core.c,
# persist.c) with ios/host_ios.c for a host; ios/*.m are the app, Metal (one
# texture: the canvas) and the sound.
#   ios-sim  build/ios-sim/OpenTrail.app (Simulator, arm64, unsigned)
#   ios      build/OpenTrail.ipa (device, arm64; signed when IOS_SIGN_IDENTITY,
#            IOS_PROFILE and IOS_TEAM_ID are set)
# ---------------------------------------------------------------------------
IOS_MIN        ?= 15.0
IOS_APP_NAME   := OpenTrail
IOS_BUNDLE_ID  := com.danheskett.opentrail
# CFBundleVersion rises with every upload: the release's number.
IOS_BUILD_NUMBER ?= $(RELEASE_VERSION)
ifeq ($(IOS_BUILD_NUMBER),0)
IOS_BUILD_NUMBER := 1
endif
IOS_VERSION_NAME ?= 1.0.$(IOS_BUILD_NUMBER)
IOS_SIGN_IDENTITY ?=
IOS_PROFILE       ?=
IOS_TEAM_ID       ?=
IOS_ICON := ios/Assets.xcassets/AppIcon.appiconset/icon-1024.png

IOS_M_SRC   := ios/ios_main.m ios/present_metal.m ios/plat_ios.m ios/audio_ios.m
IOS_C_SRC   := $(CORE_SRC) src/host/core.c src/host/persist.c ios/host_ios.c build/embed.c build/embed_art.c
IOS_CFLAGS  := $(CFLAGS_COMMON) -O2 -DPLATFORM_IOS -Iios
IOS_MFLAGS  := -fobjc-arc -Wall -Wextra -O2 -DPLATFORM_IOS -Iios -Isrc/engine -Isrc/machine -Isrc/legacy \
               -Isrc/modern -Isrc/host -Ibuild
IOS_FRAMEWORKS := -framework UIKit -framework Metal -framework QuartzCore -framework AVFoundation -framework Foundation
IOS_DEPS := $(IOS_M_SRC) $(IOS_C_SRC) $(HDRS) $(wildcard ios/*.h) ios/Info.plist build/version.h

# $(call ios_build,<sdk>,<target>,<app dir>,<obj dir>)
define ios_build
	@rm -rf $(4) && mkdir -p $(3) $(4)
	for f in $(IOS_C_SRC); do o=$(4)/$$(echo $$f | tr / _ | sed 's/\.c$$/.o/'); xcrun -sdk $(1) clang -target $(2) $(IOS_CFLAGS) -c $$f -o $$o || exit 1; done
	for f in $(IOS_M_SRC); do o=$(4)/$$(echo $$f | tr / _ | sed 's/\.m$$/.o/'); xcrun -sdk $(1) clang -target $(2) $(IOS_MFLAGS) -c $$f -o $$o || exit 1; done
	xcrun -sdk $(1) clang -target $(2) -fobjc-arc $(4)/*.o $(IOS_FRAMEWORKS) -o $(3)/$(IOS_APP_NAME)
	sed -e "s|<string>1</string>|<string>$(IOS_BUILD_NUMBER)</string>|" \
	    -e "s|<string>1.0</string>|<string>$(IOS_VERSION_NAME)</string>|" ios/Info.plist > $(3)/Info.plist
endef

IOS_SIM_APP := build/ios-sim/$(IOS_APP_NAME).app
ios-sim: $(IOS_SIM_APP)
$(IOS_SIM_APP): $(IOS_DEPS)
	$(call ios_build,iphonesimulator,arm64-apple-ios$(IOS_MIN)-simulator,$(IOS_SIM_APP),build/ios-sim/obj)
	@echo "[ios] built $@"

IOS_APP_DIR := build/ios-device/Payload/$(IOS_APP_NAME).app
IOS_IPA     := build/$(IOS_APP_NAME).ipa
ios: $(IOS_IPA)
# A hand-assembled bundle lacks the keys Xcode injects; each below is one an
# upload has been refused for (the family's notes).
$(IOS_IPA): $(IOS_DEPS)
	$(call ios_build,iphoneos,arm64-apple-ios$(IOS_MIN),$(IOS_APP_DIR),build/ios-device/obj)
	plist=$(IOS_APP_DIR)/Info.plist; \
	/usr/libexec/PlistBuddy \
	    -c "Add :CFBundleSupportedPlatforms array" -c "Add :CFBundleSupportedPlatforms:0 string iPhoneOS" \
	    -c "Add :DTPlatformName string iphoneos" \
	    -c "Add :UIRequiredDeviceCapabilities array" -c "Add :UIRequiredDeviceCapabilities:0 string arm64" \
	    -c "Set :CFBundleVersion $(IOS_BUILD_NUMBER)" -c "Set :CFBundleShortVersionString $(IOS_VERSION_NAME)" "$$plist"
	xcrun actool ios/Assets.xcassets --compile $(IOS_APP_DIR) --platform iphoneos --minimum-deployment-target $(IOS_MIN) \
	    --target-device iphone --target-device ipad --app-icon AppIcon \
	    --output-partial-info-plist build/ios-device/assetcatalog.plist >/dev/null
	/usr/libexec/PlistBuddy -c "Merge build/ios-device/assetcatalog.plist" $(IOS_APP_DIR)/Info.plist
	plutil -replace CFBundleIconName -string AppIcon $(IOS_APP_DIR)/Info.plist
	plist=$(IOS_APP_DIR)/Info.plist; \
	xcode_ver=$$(xcodebuild -version | sed -n '1s/^Xcode //p'); \
	xcode_build=$$(xcodebuild -version | sed -n '2s/^Build version //p'); \
	sdk_ver=$$(xcrun --sdk iphoneos --show-sdk-version); \
	sdk_build=$$(xcrun --sdk iphoneos --show-sdk-build-version); \
	plat_ver=$$(xcrun --sdk iphoneos --show-sdk-platform-version); \
	dtxcode=$$(echo $$xcode_ver | awk -F. '{printf "%02d%d%d", $$1, $$2+0, $$3+0}'); \
	plutil -replace DTXcode -string "$$dtxcode" $$plist; \
	plutil -replace DTXcodeBuild -string "$$xcode_build" $$plist; \
	plutil -replace DTSDKName -string "iphoneos$$sdk_ver" $$plist; \
	plutil -replace DTSDKBuild -string "$$sdk_build" $$plist; \
	plutil -replace DTPlatformVersion -string "$$plat_ver" $$plist; \
	plutil -replace DTPlatformBuild -string "$$sdk_build" $$plist; \
	plutil -replace DTCompiler -string "com.apple.compilers.llvm.clang.1_0" $$plist; \
	plutil -replace BuildMachineOSBuild -string "$$(sw_vers -buildVersion)" $$plist; \
	echo "[ios] toolchain: Xcode $$xcode_ver ($$xcode_build), iphoneos SDK $$sdk_ver ($$sdk_build)"
	@if [ -n "$(IOS_SIGN_IDENTITY)" ]; then \
	    if [ -z "$(IOS_PROFILE)" ] || [ -z "$(IOS_TEAM_ID)" ]; then echo "error: signing needs IOS_PROFILE and IOS_TEAM_ID too" >&2; exit 1; fi; \
	    cp "$(IOS_PROFILE)" $(IOS_APP_DIR)/embedded.mobileprovision; \
	    ents=build/ios-device/entitlements.plist; \
	    printf '%s\n' '<?xml version="1.0" encoding="UTF-8"?>' \
	      '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">' \
	      '<plist version="1.0"><dict>' \
	      '  <key>application-identifier</key><string>$(IOS_TEAM_ID).$(IOS_BUNDLE_ID)</string>' \
	      '  <key>com.apple.developer.team-identifier</key><string>$(IOS_TEAM_ID)</string>' \
	      '  <key>get-task-allow</key><false/>' '</dict></plist>' > $$ents; \
	    codesign --force --timestamp=none --sign "$(IOS_SIGN_IDENTITY)" --entitlements $$ents $(IOS_APP_DIR); \
	    codesign --verify --strict --verbose=2 $(IOS_APP_DIR); \
	fi
	cd build/ios-device && rm -f ../$(IOS_APP_NAME).ipa && zip -qr ../$(IOS_APP_NAME).ipa Payload
	@echo "[ios] built $@ ($(if $(IOS_SIGN_IDENTITY),signed,unsigned), build $(IOS_BUILD_NUMBER))"

dist-ios: $(IOS_IPA)
	@mkdir -p $(DIST)
	cp $(IOS_IPA) $(DIST)/$(PKG)-ios-arm64.ipa

# The iOS purity check (make test): every C file the iOS build compiles builds
# with PLATFORM_IOS and no raylib anywhere on the include path.
ios-purity: $(filter %.c,$(IOS_C_SRC)) build/version.h | build
	@for f in $(filter-out build/embed.c build/embed_art.c,$(IOS_C_SRC)); do \
	    gcc $(IOS_CFLAGS) -fsyntax-only $$f || { echo "FAIL: ios-purity $$f"; exit 1; }; done
	@echo "PASS: ios-purity: the iOS build's C needs no raylib"

.PHONY: FORCE all test clean run windows mac web web-classic web-modern web-serve dist-linux dist-windows dist-mac dist-web ios ios-sim dist-ios ios-purity android android-play dist-android dist-android-play

# The harness: the real game, either mode, stepped by commands on stdin, with
# JSON state and PNG snapshots (tools/c/harness.c; tools/harness.py drives it)
HARNESS_HOST := $(filter-out src/host/main.c src/host/persist.c,$(HOST_SRC))
build/harness: tools/c/harness.c tools/c/png.h $(CORE_SRC) $(HARNESS_HOST) build/embed.c build/embed_art.c $(HDRS) | build
	gcc $(CFLAGS_COMMON) $(REC_INC) -Itools/c -O2 -I$(RAYLIB)/include tools/c/harness.c $(CORE_SRC) $(HARNESS_HOST) build/embed.c build/embed_art.c -o $@ $(LDFLAGS)

# Modern mode's music, previewed: build/music_wav OUTDIR [tune] writes each
# tune as modern mode plays it and as the original's speaker did (WAV)
build/music_wav: tools/c/music_wav.c src/modern/music.c src/modern/music.h $(MACHINE_SRC) $(HDRS) | build
	gcc $(CFLAGS_COMMON) -O2 tools/c/music_wav.c src/modern/music.c $(MACHINE_SRC) -o $@ -lm

# Headless driver: build/play "KEYS" out.bin, then build/scr2png out.bin out.png
build/play: tools/c/play.c $(CORE_SRC) $(HDRS) build/embed_art_none.c | build
	gcc $(CFLAGS_COMMON) -O1 -g -fsanitize=address,undefined tools/c/play.c $(CORE_SRC) build/embed_art_none.c -o $@ -lm

build/scr2png: tools/c/scr2png.c tools/c/png.h src/legacy/ntsc.c | build
	gcc $(CFLAGS_COMMON) -O2 tools/c/scr2png.c src/legacy/ntsc.c -o $@

# Originals for the art pipeline: build/ima2png SET OUTPREFIX [raw]
build/ima2png: tools/c/ima2png.c tools/c/png.h $(CORE_SRC) $(HDRS) build/embed_art_none.c | build
	gcc $(CFLAGS_COMMON) -O1 tools/c/ima2png.c $(CORE_SRC) build/embed_art_none.c -o $@ -lm
