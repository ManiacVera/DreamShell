#
# DreamShell applications Makefile
# Copyright (C) 2009-2024 SWAT
# http://www.dc-swat.ru
#

_SUBDIRS = main filemanager region_changer iso_loader \
			bios_flasher gd_ripper speedtest vmu_manager \
			settings gdplay dreameye network games_menu

all: $(patsubst %, _dir_%, $(_SUBDIRS))

$(patsubst %, _dir_%, $(_SUBDIRS)):
	$(MAKE) -C $(patsubst _dir_%, %, $@) all install

clean: $(patsubst %, _clean_dir_%, $(_SUBDIRS))

$(patsubst %, _clean_dir_%, $(_SUBDIRS)):
	$(MAKE) -C $(patsubst _clean_dir_%, %, $@) clean

install: $(patsubst %, _install_dir_%, $(_SUBDIRS))

$(patsubst %, _install_dir_%, $(_SUBDIRS)):
	$(MAKE) -C $(patsubst _install_dir_%, %, $@) install
