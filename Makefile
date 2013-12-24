# (c) Copyright Paul Campbell paul@taniwha.com 2013
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) version 3, or any
# later version accepted by Paul Campbell , who shall
# act as a proxy defined in Section 6 of version 3 of the license.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
# 
# You should have received a copy of the GNU Lesser General Public 
# License along with this library.  If not, see <http://www.gnu.org/licenses/>.
#

THIS_ARCH = 1		# set this to match the hardware 
THIS_CODE_BASE = 0	# set this to match the code base currently running in the board
THIS_VERSION = 2	# suota version
# DRV_KEYS = 1		# define for key driver
# DRV_LEDS = 1		# define for leds driver
# DRV_DAYLIGHT = 1	# define for daylight detector driver

# kernel tree root for building kernels
KERNEL_ROOT=.
SERIAL_ROOT=.

#
#	where the app is
#
APP_ROOT=./sample_app
APP_NAME=app


SDCC_INSTALL_DIR = /usr/local/share/sdcc
KERNEL_ALL = packet_loader kernel.ihx  kernel.lk serial.ihx 
#KERNEL_ALL = 

all:	$(KERNEL_ALL) $(APP_NAME).ihx $(APP_NAME)_even.suota $(APP_NAME)_odd.suota 

HOST_CC = gcc
HOST_CPP = g++
CC = sdcc
LD = sdld -n
AS = sdas8051

BASE0 = 0x2000
BASE1 = 0x5000

CFLAGS = -mmcs51 --model-medium --opt-code-size --debug -I$(KERNEL_ROOT)/include -DBASE0=$(BASE0) -DBASE1=$(BASE1)
CFLAGS += -DTHIS_ARCH=$(THIS_ARCH) -DTHIS_CODE_BASE=$(THIS_CODE_BASE) -DTHIS_VERSION=$(THIS_VERSION)
ifdef DRV_KEYS
CFLAGS += -DDRV_KEYS
endif
ifdef DRV_LEDS
CFLAGS += -DDRV_LEDS
endif
ifdef DRV_DAYLIGHT
CFLAGS += -DDRV_DAYLIGHT
endif
LDLIBS_SA = -k $(SDCC_INSTALL_DIR)/lib/medium -l mcs51 -l libsdcc -l libint -l liblong -l libfloat
LDLIBS = -k $(SDCC_INSTALL_DIR)/lib/medium -l libsdcc -l libint -l liblong -l libfloat
LDFLAGS_SA = -muwx -b SSEG=0x80 $(LDLIBS_SA) -M -Y -b BSEG=9
LDFLAGS = -muwx -b SSEG=0x80 $(LDLIBS) -M -Y 
LDEVEN = -b GSINIT0=$(BASE0)
LDODD  = -b GSINIT0=$(BASE1)


# kernel objects
KOBJ = task.rel suota.rel rf.rel suota_key.rel
ifdef DRV_LEDS
KOBJ += leds.rel
endif
ifdef DRV_KEYS
KOBJ += keys.rel
endif

CFLAGS += -DAPP        
AOBJ = $(APP_NAME).rel
APP_REQUIRED=kernel.lk syms.rel app_hdr.rel fixcrc
#APP_REQUIRED=

############## App starts here ############################################
#
#	To build a SUOTA app:
#
#	- create a new app build directory
#	- copy this Makefile to the app build directory
#	- copy sample_app/app.c to app build directory
#	- change directory to the new build directory
#	- edit the Makefile
#		- edit APP_NAME 
#		- edit APP_ROOT to point to "."
#		- edit KERNEL_ROOT and SERIAL_ROOT to point to the place you installed the kernel
#		- set "THIS_ARCH" to match your hardware
#		- choose a value for "THIS_CODE_BASE" (probably this means choosing a random CODE_BASE between 1-254)
#		- edit THIS_VERSION to 1 (increment each time you make a downloadble build)
#	- type "make" to build a kernel/etc
#	- build and test your kernel and app until you want to freeze it then:
#		- make a subdirectory called 'save' and copy everything into it (just in case you lose the kernel)
#		- edit the Makefile again
#			- in the makefile remove everything below "## Kernel starts here ##"
#			- edit APP_REQUIRED to make it empty
#			- edit KERNEL_ALL to make it empty
#
#	Below is a 'push' target you can use to automatically bump the version number, build
#	and push a new image to a device
#

#
# unified build	for example application - you must include app_integrated.rel
#
$(APP_NAME).ihx:	$(KOBJ) $(AOBJ) kernel.ihx app_integrated.rel
	$(LD) $(LDFLAGS_SA) -i $(APP_NAME).ihx $(KOBJ) $(AOBJ) app_integrated.rel

#
# downloadable SUOTA builds for example application - you must include app_hdr.rel and syms.rel and -f kernel.lk
#
$(APP_NAME)_even.suota:	$(AOBJ) $(APP_REQUIRED)
	$(LD) $(LDEVEN) $(LDFLAGS) -f kernel.lk -i $(APP_NAME)_even.ihx  $(AOBJ) syms.rel app_hdr.rel
	./fixcrc -v $(THIS_VERSION) <$(APP_NAME)_even.ihx >$(APP_NAME)_even.suota

$(APP_NAME)_odd.suota:	$(AOBJ) $(APP_REQUIRED)
	$(LD) $(LDODD) $(LDFLAGS) -f kernel.lk -i $(APP_NAME)_odd.ihx  $(AOBJ) syms.rel app_hdr.rel
	./fixcrc -v $(THIS_VERSION) <$(APP_NAME)_odd.ihx >$(APP_NAME)_odd.suota

#
# 	example application
#
$(APP_NAME).rel:	$(APP_ROOT)/$(APP_NAME).c  $(KERNEL_ROOT)/include/interface.h 
	$(CC) $(CFLAGS) -c $(APP_ROOT)/$(APP_NAME).c -o $(APP_NAME).rel

#
#	code to automatically build and push new software versions of code to
#	a single remote device.
#
#	Put the key you want to SUOTA with and the channel it happens on below, use the
#	command 
#	
#		"make push"
#
#	to build new code and push it to a device
#
APP_SUOTA_KEY=-k ccb17cb57448634ce595c15acf966145
APP_SUOTA_CHANNEL=11

push:	$(AOBJ) $(APP_REQUIRED) THIS_VERSION packet_loader
	version=`cat ./THIS_VERSION` ; \
	version=$$(($$version+1));\
	odd=$$(($$version%2));\
	if test $$odd -eq 1; then \
		$(LD) $(LDODD) $(LDFLAGS) -f kernel.lk -i tmp.ihx  $(AOBJ) syms.rel app_hdr.rel;\
	else \
		$(LD) $(LDEVEN) $(LDFLAGS) -f kernel.lk -i tmp.ihx  $(AOBJ) syms.rel app_hdr.rel;\
	fi ; \
	./fixcrc -v $$version $(APP_SUOTA_KEY) <tmp.ihx >$(APP_NAME).suota;\
	echo $$version >THIS_VERSION ; \
	echo Installing version $$version ; \
	./packet_loader -c $(APP_SUOTA_CHANNEL) -x $(APP_NAME).suota

p:
	version=`cat ./THIS_VERSION` ; \
	echo Installing version $$version ; \
	./packet_loader -c $(APP_SUOTA_CHANNEL) -x $(APP_NAME).suota

THIS_VERSION:
	echo $(THIS_VERSION) >THIS_VERSION


############## Kernel starts here ############################################
#
#	To build a kernel for SUOTA:
#
#		- edit the "THIS_" variables above to suit set the version to 0
#		- edit LDLIBS/LDLIBS_SA to point to where your SDCC libraries were installed
#		- change HOST_CC/HOST_CPP to point to your local native compilers
#		- type "make kernel"
#		- copy the folowing files to your app build directory:
#			- kernel.lk
#			- app_hdr.rel
#			- syms.rel
#			- fixcrc
#		- burn the kernel.ihx file into your hardware
#
kernel:	kernel.lk app_hdr.rel syms.rel fixcrc

#
#	create loader script to export kernel context to loadable apps
#
kernel.lk:	kernel.map $(KERNEL_ROOT)/kernel/map.pl
	perl $(KERNEL_ROOT)/kernel/map.pl <kernel.map >kernel.lk
#
#	build kernel - kernel.ihx is what you build into a board (we'll add somethign here to allow
#		later patching of MAC addresses)
#
kernel.ihx kernel.map:	$(KOBJ) dummy.rel
	$(LD)  $(LDFLAGS_SA) -l mcs51 -b PSEG=0 -b XSEG=0x100 -i kernel.ihx $(KOBJ) dummy.rel 


#
#	build kernel objects - leds.rel and kerys.rel should be left out of you don't need them
#
task.rel:	$(KERNEL_ROOT)/kernel/task.c  $(KERNEL_ROOT)/include/rf.h $(KERNEL_ROOT)/include/task.h $(KERNEL_ROOT)/include/interface.h  $(KERNEL_ROOT)/include/suota.h $(KERNEL_ROOT)/include/protocol.h
	$(CC) $(CFLAGS) -c $(KERNEL_ROOT)/kernel/task.c
rf.rel:	$(KERNEL_ROOT)/kernel/rf.c  $(KERNEL_ROOT)/include/rf.h $(KERNEL_ROOT)/include/task.h $(KERNEL_ROOT)/include/interface.h  $(KERNEL_ROOT)/include/suota.h $(KERNEL_ROOT)/include/protocol.h
	$(CC) $(CFLAGS) -c $(KERNEL_ROOT)/kernel/rf.c
keys.rel:	$(KERNEL_ROOT)/kernel/keys.c  $(KERNEL_ROOT)/include/rf.h $(KERNEL_ROOT)/include/task.h $(KERNEL_ROOT)/include/interface.h  $(KERNEL_ROOT)/include/suota.h $(KERNEL_ROOT)/include/protocol.h
	$(CC) $(CFLAGS) -c $(KERNEL_ROOT)/kernel/keys.c
suota.rel:	$(KERNEL_ROOT)/kernel/suota.c  $(KERNEL_ROOT)/include/rf.h $(KERNEL_ROOT)/include/task.h $(KERNEL_ROOT)/include/interface.h  $(KERNEL_ROOT)/include/suota.h $(KERNEL_ROOT)/include/protocol.h
	$(CC) $(CFLAGS) -c $(KERNEL_ROOT)/kernel/suota.c
suota_key.rel:	$(KERNEL_ROOT)/kernel/suota_key.c		#change this if you want a private efault suota key
	$(CC) $(CFLAGS) -c $(KERNEL_ROOT)/kernel/suota_key.c
leds.rel:	$(KERNEL_ROOT)/kernel/leds.s  
	$(AS)   -z -l -o leds.rel $(KERNEL_ROOT)/kernel/leds.s

#
#	this file provides access to kernel symbols exported using the linker .lk file - this
#		is solely to avoid a bug in sdcc's linker which complains if you have a -g
#		directive to declare a global variable but never use it
#
syms.rel:	$(KERNEL_ROOT)/kernel/syms.c  
	$(CC) $(CFLAGS) -c $(KERNEL_ROOT)/kernel/syms.c

#
#	code header for unified build
#
app_integrated.rel:	$(KERNEL_ROOT)/kernel/app_integrated.c  $(KERNEL_ROOT)/include/rf.h $(KERNEL_ROOT)/include/task.h $(KERNEL_ROOT)/include/interface.h  $(KERNEL_ROOT)/include/suota.h $(KERNEL_ROOT)/include/protocol.h
	$(CC) $(CFLAGS) -c $(KERNEL_ROOT)/kernel/app_integrated.c

#
#	code header for SUOTA apps
#
app_hdr.rel:	$(KERNEL_ROOT)/kernel/app_hdr.c  $(KERNEL_ROOT)/include/rf.h $(KERNEL_ROOT)/include/task.h $(KERNEL_ROOT)/include/interface.h  $(KERNEL_ROOT)/include/suota.h $(KERNEL_ROOT)/include/protocol.h
	$(CC) $(CFLAGS) -c $(KERNEL_ROOT)/kernel/app_hdr.c

#
#	dummy code header for kernel build
#
dummy.rel:	$(KERNEL_ROOT)/kernel/dummy.c  $(KERNEL_ROOT)/include/protocol.h
	$(CC) $(CFLAGS) -c $(KERNEL_ROOT)/kernel/dummy.c

#
#	app to fill in code headers in SUOTA builds and create loadable binaries
#
fixcrc:	$(KERNEL_ROOT)/kernel/fixcrc.c
	$(HOST_CC) -o fixcrc $(KERNEL_ROOT)/kernel/fixcrc.c


#
#	linux side packet interface code for talking to the serial internet gateway
#	plus example interactive version - this app will host a SUOTA server for your
#	network
#
packet_loader:	packet_interface.o packet_loader.o 
	$(HOST_CPP) -o packet_loader packet_interface.o packet_loader.o -lpthread -g -lz
packet_loader.o:	$(SERIAL_ROOT)/serial/packet_interface.h $(SERIAL_ROOT)/serial/test.cpp  $(SERIAL_ROOT)/serial/ip_packet_interface.h
	$(HOST_CPP) -c $(SERIAL_ROOT)/serial/test.cpp -o packet_loader.o -I $(KERNEL_ROOT)/include -I $(SERIAL_ROOT)/serial -g 
packet_interface.o:	$(SERIAL_ROOT)/serial/packet_interface.h $(SERIAL_ROOT)/serial/packet_interface.cpp
	$(HOST_CPP) -c $(SERIAL_ROOT)/serial/packet_interface.cpp -I $(KERNEL_ROOT)/include -I $(SERIAL_ROOT)/serial -g
ip_packet_interface.o:	$(SERIAL_ROOT)/serial/packet_interface.h $(SERIAL_ROOT)/serial/ip_packet_interface.cpp $(SERIAL_ROOT)/serial/ip_packet_interface.h
	$(HOST_CPP) -c $(SERIAL_ROOT)/serial/ip_packet_interface.cpp -I $(KERNEL_ROOT)/include -I $(SERIAL_ROOT)/serial -g


#
#
#	unified serial app for the dev board that turns it into a packet gateway
#
SOBJ = serial_app.rel app_integrated.rel
serial.ihx:	$(KOBJ) packet_interface.o $(SOBJ) 
	$(LD) $(LDFLAGS_SA) -i serial.ihx $(KOBJ) $(SOBJ)
serial_app.rel:	$(SERIAL_ROOT)/serial/serial_app.c  $(KERNEL_ROOT)/include/rf.h $(KERNEL_ROOT)/include/task.h $(KERNEL_ROOT)/include/interface.h  $(KERNEL_ROOT)/include/suota.h $(SERIAL_ROOT)/serial/packet_interface.h $(KERNEL_ROOT)/include/protocol.h
	$(CC) $(CFLAGS) -c $(SERIAL_ROOT)/serial/serial_app.c



#
#	how to build standalone code without the kernel
#
TOBJ = test.rel  leds.rel
test:	$(TOBJ)
	$(LD) $(LDFLAGS_SA) -i test $(TOBJ)

test.rel:	test.c  
	$(CC) $(CFLAGS) -c test.c

clean:
	rm -f *.rel *.map *.lst *.ihx *.asm *.mem *.sym *.lk *.rst *.o *.cdb *.adb *.omf packet_loader fixcrc *.suota


#
#	test code for Paul
#

SUOTA_KEY=ccb17cb57448634ce595c15acf966145

suota_test:	test2_even.suota test3_odd.suota test4_even.suota test5_odd.suota
test2_even.suota:	test2.rel kernel.lk syms.rel app_hdr.rel fixcrc
	$(LD) $(LDEVEN) $(LDFLAGS) -f kernel.lk -i test2.ihx  test2.rel syms.rel app_hdr.rel
	./fixcrc -v 2 -k $(SUOTA_KEY) <test2.ihx >test2_even.suota
test2.rel:	$(APP_ROOT)/app.c  $(KERNEL_ROOT)/include/interface.h 
	$(CC) $(CFLAGS) -c $(APP_ROOT)/app.c -o test2.rel -DVV="\"Test 2\n\"" -DVER=2

test3_odd.suota:	test3.rel kernel.lk syms.rel app_hdr.rel fixcrc
	$(LD) $(LDODD) $(LDFLAGS) -f kernel.lk -i test3.ihx  test3.rel syms.rel app_hdr.rel
	./fixcrc -v 3 -k $(SUOTA_KEY) <test3.ihx >test3_odd.suota
test3.rel:	$(APP_ROOT)/app.c  $(KERNEL_ROOT)/include/interface.h 
	$(CC) $(CFLAGS) -c $(APP_ROOT)/app.c -o test3.rel -DVV="\"Test 3\n\"" -DVER=3

test4_even.suota:	test4.rel kernel.lk syms.rel app_hdr.rel fixcrc
	$(LD) $(LDEVEN) $(LDFLAGS) -f kernel.lk -i test4.ihx  test4.rel syms.rel app_hdr.rel
	./fixcrc -v 4 -k $(SUOTA_KEY) <test4.ihx >test4_even.suota
test4.rel:	$(APP_ROOT)/app.c  $(KERNEL_ROOT)/include/interface.h 
	$(CC) $(CFLAGS) -c $(APP_ROOT)/app.c -o test4.rel -DVV="\"Test 4\n\"" -DVER=4

test5_odd.suota:	test5.rel kernel.lk syms.rel app_hdr.rel fixcrc
	$(LD) $(LDODD) $(LDFLAGS) -f kernel.lk -i test5.ihx  test5.rel syms.rel app_hdr.rel
	./fixcrc -v 5 -k $(SUOTA_KEY) <test5.ihx >test5_odd.suota
test5.rel:	$(APP_ROOT)/app.c  $(KERNEL_ROOT)/include/interface.h 
	$(CC) $(CFLAGS) -c $(APP_ROOT)/app.c -o test5.rel -DVV="\"Test 5\n\"" -DVER=5

