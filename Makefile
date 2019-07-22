# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the unofficial Marvell PPv2 driver port.
#

SRCDIR=	${.CURDIR}

.PATH:	${SRCDIR}

KMOD=	pepevtwo

SRCS=\
	src/if_pepevtwo.c \
	src/mvpp2_main.c \
	src/mvpp2_prs.c \
	src/mvpp2_cls.c

CLEANFILES+= ${KMOD}.ko.full ${KMOD}.ko.debug

CFLAGS+= -I${SYSDIR}/compat/linuxkpi/common/include

CFLAGS+= '-DKBUILD_MODNAME="${KMOD}"'

SRCS	+=			\
	opt_acpi.h \
	opt_inet.h \
	acpi_if.h \
	bus_if.h \
	device_if.h \
	pci_if.h \
	vnode_if.h

.include <bsd.kmod.mk>
