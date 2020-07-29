.PHONY: default install clean

BUILD = build

default: $(BUILD)/Makefile
	@cd $(BUILD) && $(MAKE)

install: default
	@cd $(BUILD) && cpack
	@dpkg --install $(BUILD)/*.dpkg
clean:
	@rm -rf $(BUILD)

$(BUILD)/Makefile:
	@mkdir -p $(BUILD)
	@cd $(BUILD) && cmake ..
