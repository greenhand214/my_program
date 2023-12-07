# @echo "Write this Makefile by your self."
# VSRC = $(wildcard ./vsrc/*.v)
# CSRC = $(wildcard ./csrc/*.cpp)
VSRC = $(wildcard ./vsrc/our_OnOff.v)
CSRC = $(wildcard ./csrc/main.cpp)
all:
	@echo "Write this Makefile by your self."
 
sim:
	$(call git_commit, "sim RTL") # DO NOT REMOVE THIS LINE!!!
	verilator -Wno-fatal $(VSRC) $(CSRC) --top-module our_OnOff --cc --trace --exe
	make -C obj_dir -f Vour_OnOff.mk Vour_OnOff
	./obj_dir/Vour_OnOff
	gtkwave wave.vcd
 
 .PHONY:clean
clean:
	rm -rf obj_dir wave.vcd	
