.PHONY: all clean

all: release
osdmenu: osdmenu.elf
hosdmenu: hosdmenu.elf
osdmbr: osdmbr.elf osdmbr-installer.elf
launcher: launcher.elf

release: clean osdmenu hosdmenu osdmbr launcher
	mkdir -p release/
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
	mkdir -p release/patcher/kelf
# OSDMenu + PSU
	cp patcher/patcher.elf release/osdmenu.elf
	python3 utils/psu/makepsu.py release/SYS_OSDMENU.psu SYS_OSDMENU utils/psu/res release/osdmenu.elf
# OSDMenu KELF
	cp patcher/PATCHER.XLF release/patcher/kelf/OSDMENU.XLF
	cp -r utils/icn/* release/patcher/kelf
	cp patcher/README.md release/patcher/README.md
	cp README.md release/

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
	cp mbr/README.md release/osdmbr/

# OSDMenu MBR Installer
osdmbr-installer.elf:
	$(MAKE) -C utils/installer
	mkdir -p release/osdmbr/
	cp utils/installer/osdmbr-installer.elf release/osdmbr/

# OSDMenu Launcher
launcher.elf:
	$(MAKE) -C launcher clean
	$(MAKE) -C launcher kelf STANDALONE=1
	mkdir -p release/launcher
	cp launcher/LAUNCHER.XLF release/launcher/
	cp launcher/launcher.elf release/launcher/
	cp launcher/README.md release/launcher/
