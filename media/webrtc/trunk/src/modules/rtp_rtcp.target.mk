# This file is generated by gyp; do not edit.

TOOLSET := target
TARGET := rtp_rtcp
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
	-Isrc/modules/rtp_rtcp/interface \
	-Isrc/modules/interface \
	-Isrc/system_wrappers/interface

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
	-Isrc/modules/rtp_rtcp/interface \
	-Isrc/modules/interface \
	-Isrc/system_wrappers/interface

OBJS := $(obj).target/$(TARGET)/src/modules/rtp_rtcp/source/bitrate.o \
	$(obj).target/$(TARGET)/src/modules/rtp_rtcp/source/rtp_rtcp_impl.o \
	$(obj).target/$(TARGET)/src/modules/rtp_rtcp/source/rtcp_receiver.o \
	$(obj).target/$(TARGET)/src/modules/rtp_rtcp/source/rtcp_receiver_help.o \
	$(obj).target/$(TARGET)/src/modules/rtp_rtcp/source/rtcp_sender.o \
	$(obj).target/$(TARGET)/src/modules/rtp_rtcp/source/rtcp_utility.o \
	$(obj).target/$(TARGET)/src/modules/rtp_rtcp/source/rtp_receiver.o \
	$(obj).target/$(TARGET)/src/modules/rtp_rtcp/source/rtp_sender.o \
	$(obj).target/$(TARGET)/src/modules/rtp_rtcp/source/rtp_utility.o \
	$(obj).target/$(TARGET)/src/modules/rtp_rtcp/source/ssrc_database.o \
	$(obj).target/$(TARGET)/src/modules/rtp_rtcp/source/tmmbr_help.o \
	$(obj).target/$(TARGET)/src/modules/rtp_rtcp/source/dtmf_queue.o \
	$(obj).target/$(TARGET)/src/modules/rtp_rtcp/source/rtp_receiver_audio.o \
	$(obj).target/$(TARGET)/src/modules/rtp_rtcp/source/rtp_sender_audio.o \
	$(obj).target/$(TARGET)/src/modules/rtp_rtcp/source/bandwidth_management.o \
	$(obj).target/$(TARGET)/src/modules/rtp_rtcp/source/forward_error_correction.o \
	$(obj).target/$(TARGET)/src/modules/rtp_rtcp/source/forward_error_correction_internal.o \
	$(obj).target/$(TARGET)/src/modules/rtp_rtcp/source/overuse_detector.o \
	$(obj).target/$(TARGET)/src/modules/rtp_rtcp/source/h263_information.o \
	$(obj).target/$(TARGET)/src/modules/rtp_rtcp/source/remote_rate_control.o \
	$(obj).target/$(TARGET)/src/modules/rtp_rtcp/source/rtp_receiver_video.o \
	$(obj).target/$(TARGET)/src/modules/rtp_rtcp/source/rtp_sender_video.o \
	$(obj).target/$(TARGET)/src/modules/rtp_rtcp/source/receiver_fec.o \
	$(obj).target/$(TARGET)/src/modules/rtp_rtcp/source/rtp_format_vp8.o

# Add to the list of files we specially track dependencies for.
all_deps += $(OBJS)

# CFLAGS et al overrides must be target-local.
# See "Target-specific Variable Values" in the GNU Make manual.
$(OBJS): TOOLSET := $(TOOLSET)
$(OBJS): GYP_CFLAGS := $(DEFS_$(BUILDTYPE)) $(INCS_$(BUILDTYPE)) $(CFLAGS_$(BUILDTYPE)) $(CFLAGS_C_$(BUILDTYPE))
$(OBJS): GYP_CXXFLAGS := $(DEFS_$(BUILDTYPE)) $(INCS_$(BUILDTYPE)) $(CFLAGS_$(BUILDTYPE)) $(CFLAGS_CC_$(BUILDTYPE))

# Suffix rules, putting all outputs into $(obj).

$(obj).$(TOOLSET)/$(TARGET)/%.o: $(srcdir)/%.cc FORCE_DO_CMD
	@$(call do_cmd,cxx,1)

# Try building from generated source, too.

$(obj).$(TOOLSET)/$(TARGET)/%.o: $(obj).$(TOOLSET)/%.cc FORCE_DO_CMD
	@$(call do_cmd,cxx,1)

$(obj).$(TOOLSET)/$(TARGET)/%.o: $(obj)/%.cc FORCE_DO_CMD
	@$(call do_cmd,cxx,1)

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

$(obj).target/src/modules/librtp_rtcp.a: GYP_LDFLAGS := $(LDFLAGS_$(BUILDTYPE))
$(obj).target/src/modules/librtp_rtcp.a: LIBS := $(LIBS)
$(obj).target/src/modules/librtp_rtcp.a: TOOLSET := $(TOOLSET)
$(obj).target/src/modules/librtp_rtcp.a: $(OBJS) FORCE_DO_CMD
	$(call do_cmd,alink)

all_deps += $(obj).target/src/modules/librtp_rtcp.a
# Add target alias
.PHONY: rtp_rtcp
rtp_rtcp: $(obj).target/src/modules/librtp_rtcp.a

# Add target alias to "all" target.
.PHONY: all
all: rtp_rtcp

