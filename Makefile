.PHONY: all clean

all: osdmenu hosdmenu
osdmenu: osdmenu.elf
hosdmenu: hosdmenu.elf

clean:
	rm osdmenu.elf hosdmenu.elf
	$(MAKE) -C patcher clean

# OSDMenu
osdmenu.elf:
	$(MAKE) -C patcher clean
	$(MAKE) -C patcher/$<
	cp patcher/patcher.elf osdmenu.elf

# HOSDMenu
hosdmenu.elf:
	$(MAKE) -C patcher clean
	$(MAKE) -C patcher/$< HOSD=1
	cp patcher/patcher.elf hosdmenu.elf
