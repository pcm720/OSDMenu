.PHONY: all clean

all: release
osdmenu: osdmenu.elf
hosdmenu: hosdmenu.elf
osdmbr: osdmbr.elf osdmbr-installer.elf

release: clean osdmenu hosdmenu osdmbr
	mkdir -p release/
	cp README.md release/
	cp mbr/README.md release/osdmbr/
	cp -r examples release/

clean:
	rm -rf release
	$(MAKE) -C launcher clean
	$(MAKE) -C patcher clean
	$(MAKE) -C mbr clean
	$(MAKE) -C utils/installer clean

# OSDMenu
osdmenu.elf:
	$(MAKE) -C patcher clean
	$(MAKE) -C patcher kelf
	mkdir -p release/kelf
# OSDMenu + PSU
	cp patcher/patcher.elf release/osdmenu.elf
	python3 utils/psu/makepsu.py release/SYS_OSDMENU.psu SYS_OSDMENU utils/psu/res release/osdmenu.elf
# OSDMenu KELF
	cp patcher/PATCHER.XLF release/kelf/OSDMENU.XLF
	cp -r utils/icn/* release/kelf

# HOSDMenu
hosdmenu.elf:
	$(MAKE) -C patcher clean
	$(MAKE) -C patcher HOSD=1
	mkdir -p release/
	cp patcher/patcher.elf release/hosdmenu.elf

# OSDMenu MBR
osdmbr.elf:
	$(MAKE) -C mbr clean
	$(MAKE) -C mbr kelf
	mkdir -p release/osdmbr/payloads
	cp mbr/OSDMBR.XLF release/osdmbr/
	cp mbr/osdmbr.elf release/osdmbr/payloads
	cp mbr/osdmbr.bin release/osdmbr/payloads

# OSDMenu MBR Installer
osdmbr-installer.elf:
	$(MAKE) -C utils/installer
	mkdir -p release/osdmbr/
	cp utils/installer/osdmbr-installer.elf release/osdmbr/

