ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

USEFILE=

include $(MKFILES_ROOT)/qmacros.mk

# Suppress the _g suffix from the debug variant
BUILDNAME=$(IMAGE_PREF_$(BUILD_TYPE))$(NAME)$(IMAGE_SUFF_$(BUILD_TYPE))

# Compiler options for enhanced security
CCFLAGS+=-fstack-protector-all -D_FORTIFY_SOURCE=2 \
	$(if $(filter g so shared,$(VARIANTS)),,-fPIE)

# Linker options for enhanced security
LDFLAGS+=-Wl,-z,relro -Wl,-z,now $(if $(filter g so shared,$(VARIANTS)),,-pie)

# Basic libraries required by most native applications
LIBS+=glview bps screen GLESv1_CM m

include $(MKFILES_ROOT)/qtargets.mk

OPTIMIZE_TYPE_g=none
OPTIMIZE_TYPE=$(OPTIMIZE_TYPE_$(filter g, $(VARIANTS)))

-include $(PROJECT_ROOT)/../samples.mk
