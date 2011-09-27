# This file is generated by gyp; do not edit.

TOOLSET := target
TARGET := iSAC
DEFS_Debug := '-DNO_HEAPCHECKER' \
	'-DCHROMIUM_BUILD' \
	'-DENABLE_REMOTING=1' \
	'-DENABLE_P2P_APIS=1' \
	'-DENABLE_CONFIGURATION_POLICY' \
	'-DENABLE_GPU=1' \
	'-DENABLE_EGLIMAGE=1' \
	'-DUSE_SKIA=1' \
	'-DENABLE_REGISTER_PROTOCOL_HANDLER=1' \
	'-DWEBRTC_TARGET_PC' \
	'-DWEBRTC_LINUX' \
	'-DWEBRTC_THREAD_RR' \
	'-D__STDC_FORMAT_MACROS' \
	'-DDYNAMIC_ANNOTATIONS_ENABLED=1' \
	'-DWTF_USE_DYNAMIC_ANNOTATIONS=1' \
	'-D_DEBUG'

# Flags passed to all source files.
CFLAGS_Debug := -Werror \
	-pthread \
	-fno-exceptions \
	-Wall \
	-Wno-unused-parameter \
	-Wno-missing-field-initializers \
	-D_FILE_OFFSET_BITS=64 \
	-fvisibility=hidden \
	-pipe \
	-fPIC \
	-fno-strict-aliasing \
	-O0 \
	-g

# Flags passed to only C files.
CFLAGS_C_Debug := 

# Flags passed to only C++ files.
CFLAGS_CC_Debug := -fno-rtti \
	-fno-threadsafe-statics \
	-fvisibility-inlines-hidden \
	-Wsign-compare

INCS_Debug := -Isrc \
	-I. \
	-Isrc/modules/audio_coding/codecs/iSAC/main/interface \
	-Isrc/common_audio/signal_processing_library/main/interface

DEFS_Release := '-DNO_HEAPCHECKER' \
	'-DCHROMIUM_BUILD' \
	'-DENABLE_REMOTING=1' \
	'-DENABLE_P2P_APIS=1' \
	'-DENABLE_CONFIGURATION_POLICY' \
	'-DENABLE_GPU=1' \
	'-DENABLE_EGLIMAGE=1' \
	'-DUSE_SKIA=1' \
	'-DENABLE_REGISTER_PROTOCOL_HANDLER=1' \
	'-DWEBRTC_TARGET_PC' \
	'-DWEBRTC_LINUX' \
	'-DWEBRTC_THREAD_RR' \
	'-D__STDC_FORMAT_MACROS' \
	'-DNDEBUG' \
	'-DNVALGRIND' \
	'-DDYNAMIC_ANNOTATIONS_ENABLED=0'

# Flags passed to all source files.
CFLAGS_Release := -Werror \
	-pthread \
	-fno-exceptions \
	-Wall \
	-Wno-unused-parameter \
	-Wno-missing-field-initializers \
	-D_FILE_OFFSET_BITS=64 \
	-fvisibility=hidden \
	-pipe \
	-fPIC \
	-fno-strict-aliasing \
	-O2 \
	-fno-ident \
	-fdata-sections \
	-ffunction-sections

# Flags passed to only C files.
CFLAGS_C_Release := 

# Flags passed to only C++ files.
CFLAGS_CC_Release := -fno-rtti \
	-fno-threadsafe-statics \
	-fvisibility-inlines-hidden \
	-Wsign-compare

INCS_Release := -Isrc \
	-I. \
	-Isrc/modules/audio_coding/codecs/iSAC/main/interface \
	-Isrc/common_audio/signal_processing_library/main/interface

OBJS := $(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/arith_routines.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/arith_routines_hist.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/arith_routines_logist.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/bandwidth_estimator.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/crc.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/decode.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/decode_bwe.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/encode.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/encode_lpc_swb.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/entropy_coding.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/fft.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/filter_functions.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/filterbank_tables.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/intialize.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/isac.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/filterbanks.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/pitch_lag_tables.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/lattice.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/lpc_gain_swb_tables.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/lpc_analysis.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/lpc_shape_swb12_tables.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/lpc_shape_swb16_tables.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/lpc_tables.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/pitch_estimator.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/pitch_filter.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/pitch_gain_tables.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/spectrum_ar_model_tables.o \
	$(obj).target/$(TARGET)/src/modules/audio_coding/codecs/iSAC/main/source/transform.o

# Add to the list of files we specially track dependencies for.
all_deps += $(OBJS)

# CFLAGS et al overrides must be target-local.
# See "Target-specific Variable Values" in the GNU Make manual.
$(OBJS): TOOLSET := $(TOOLSET)
$(OBJS): GYP_CFLAGS := $(DEFS_$(BUILDTYPE)) $(INCS_$(BUILDTYPE)) $(CFLAGS_$(BUILDTYPE)) $(CFLAGS_C_$(BUILDTYPE))
$(OBJS): GYP_CXXFLAGS := $(DEFS_$(BUILDTYPE)) $(INCS_$(BUILDTYPE)) $(CFLAGS_$(BUILDTYPE)) $(CFLAGS_CC_$(BUILDTYPE))

# Suffix rules, putting all outputs into $(obj).

$(obj).$(TOOLSET)/$(TARGET)/%.o: $(srcdir)/%.c FORCE_DO_CMD
	@$(call do_cmd,cc,1)

# Try building from generated source, too.

$(obj).$(TOOLSET)/$(TARGET)/%.o: $(obj).$(TOOLSET)/%.c FORCE_DO_CMD
	@$(call do_cmd,cc,1)

$(obj).$(TOOLSET)/$(TARGET)/%.o: $(obj)/%.c FORCE_DO_CMD
	@$(call do_cmd,cc,1)

# End of this set of suffix rules
### Rules for final target.
LDFLAGS_Debug := -pthread \
	-Wl,-z,noexecstack

LDFLAGS_Release := -pthread \
	-Wl,-z,noexecstack \
	-Wl,-O1 \
	-Wl,--as-needed \
	-Wl,--gc-sections

LIBS := 

$(obj).target/src/modules/libiSAC.a: GYP_LDFLAGS := $(LDFLAGS_$(BUILDTYPE))
$(obj).target/src/modules/libiSAC.a: LIBS := $(LIBS)
$(obj).target/src/modules/libiSAC.a: TOOLSET := $(TOOLSET)
$(obj).target/src/modules/libiSAC.a: $(OBJS) FORCE_DO_CMD
	$(call do_cmd,alink)

all_deps += $(obj).target/src/modules/libiSAC.a
# Add target alias
.PHONY: iSAC
iSAC: $(obj).target/src/modules/libiSAC.a

# Add target alias to "all" target.
.PHONY: all
all: iSAC

