EXTRA_CLEAN=$(BUILDNAME).bar

BAR_CONFIG=$(BAR_CONFIG_$(CPU)_$(subst .,_,$(subst -,_,$(COMPOUND_VARIANT))))
BAR_CONFIG_arm_o_le_v7:=Device-Release
BAR_CONFIG_arm_o_le_v7_g:=Device-Debug
BAR_CONFIG_x86_o:=Simulator
BAR_CONFIG_x86_o_g:=Simulator-Debug

bar: $(BUILDNAME).bar

$(BUILDNAME).bar: $(FULLNAME) $(PROJECT_ROOT)/bar-descriptor.xml
	cd $(PROJECT_ROOT) ; \
	blackberry-nativepackager -devMode -package $(CURDIR)/$(BUILDNAME).bar bar-descriptor.xml -configuration $(BAR_CONFIG)

deploy: $(BUILDNAME).bar
	blackberry-deploy -installApp -device $(DEVICEIP) -password $(DEVICEPW) -package $(BUILDNAME).bar

run: $(BUILDNAME).bar
	blackberry-deploy -installApp -launchApp -device $(DEVICEIP) -password $(DEVICEPW) -package $(BUILDNAME).bar
