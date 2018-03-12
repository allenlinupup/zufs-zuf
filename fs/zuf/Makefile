#
# ZUF: Zero-copy User-mode Feeder
#
# Copyright (c) 2018 NetApp Inc. All rights reserved.
#
# ZUFS-License: GPL-2.0 OR BSD-3-Clause. See module.c for LICENSE details.
#
# Makefile for the Linux zufs Kernel Feeder.
#

obj-$(CONFIG_ZUF) += zuf.o

# Infrastructure
zuf-y += md.o t2.o t1.o

# ZUF core
zuf-y += zuf-core.o zuf-root.o

# Main FS
zuf-y += super.o inode.o directory.o file.o namei.o symlink.o
zuf-y += module.o
