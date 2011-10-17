ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

NAME=bblocks

USEFILE=

LIBS=bps screen pps scoreloopcore sqlite3 asound OpenAL alut curl ssl icui18n box2d freetype m png screen EGL GLESv1_CM

LIBPREF_bps=-Bstatic
LIBPOST_bps=-Bdynamic

LIBPREF_scoreloop=-Bstatic
LIBPOST_scoreloop=-Bdynamic

EXTRA_INCVPATH=$(QNX_TARGET)/../target-override/usr/include
EXTRA_INCVPATH+=$(QNX_TARGET)/usr/include/freetype2
EXTRA_INCVPATH+=$(PROJECT_ROOT)/3rdparty/box2d

EXTRA_LIBVPATH=$(QNX_TARGET)/../target-override/$(CPUDIR)$(CPUVARDIR_SUFFIX)/usr/lib
EXTRA_SRCVPATH=$(PROJECT_ROOT)/src

CCFLAGS+=-w9 -Werror -Y_cpp-ne
OPTIMIZE_TYPE=$(if $(filter g, $(VARIANT_LIST)),NONE,SIZE)


define PINFO
PINFO DESCRIPTION=Belligerent Blocks
endef

include $(MKFILES_ROOT)/qtargets.mk
