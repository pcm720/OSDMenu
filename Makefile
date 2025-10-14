.PHONY: all clean

all: release
osdmenu: osdmenu.elf
hosdmenu: hosdmenu.elf
osdmbr: osdmbr.elf osdmbr-installer.elf

release: clean osdmenu hosdmenu osdmbr
	mkdir -p release/kelf
	mv osdmenu.elf release/
	mv hosdmenu.elf release
	mv SYS_OSDMENU.psu release/
	cp -r examples release/
	cp README.md release/
	cp mbr/README.md osdmbr/
	mv osdmbr release/
	mv OSDMENU.KELF release/kelf
	mv release/osdmbr/OSDMBR.KELF release/kelf

clean:
	rm -f osdmenu.elf hosdmenu.elf
	rm -rf osdmbr release
	$(MAKE) -C launcher clean
	$(MAKE) -C patcher clean
	$(MAKE) -C mbr clean
	$(MAKE) -C utils/installer clean

# OSDMenu
osdmenu.elf:
	$(MAKE) -C patcher clean
	$(MAKE) -C patcher kelf
	cp patcher/patcher.elf osdmenu.elf
	cp patcher/PATCHER.KELF OSDMENU.KELF
	python3 utils/psu/makepsu.py SYS_OSDMENU.psu SYS_OSDMENU utils/psu/res osdmenu.elf

# HOSDMenu
hosdmenu.elf:
	$(MAKE) -C patcher clean
	$(MAKE) -C patcher HOSD=1
	cp patcher/patcher.elf hosdmenu.elf

# OSDMenu MBR
osdmbr.elf:
	$(MAKE) -C mbr clean
	$(MAKE) -C mbr kelf
	mkdir osdmbr
	cp mbr/osdmbr.elf osdmbr/
	cp mbr/osdmbr.bin osdmbr/
	cp mbr/OSDMBR.KELF osdmbr/

# OSDMenu MBR Installer
osdmbr-installer.elf:
	$(MAKE) -C utils/installer
	cp utils/installer/osdmbr-installer.elf osdmbr/

