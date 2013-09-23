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

THIS_ARCH = 1		# set this to match the kernel in your board
THIS_CODE_BASE = 0	# set this to matchy the code base currently running in the board
THIS_VERSION = 2	# suota version
# DRV_KEYS = 1		# define for key driver
# DRV_LEDS = 1		# define for leds driver
# DRV_DAYLIGHT = 1	# define for daylight detector driver

all:	packet_loader kernel.ihx  kernel.lk serial.ihx app.ihx app_even.suota app_odd.suota 

HOST_CC = gcc
HOST_CPP = g++
CC = sdcc
LD = sdld -n
AS = sdas8051

BASE0 = 0x2000
BASE1 = 0x5000

CFLAGS = -mmcs51 --model-medium --opt-code-size --debug -Iinclude -DBASE0=$(BASE0) -DBASE1=$(BASE1)
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
LDLIBS_SA = -k /usr/local/bin/../share/sdcc/lib/medium -k /usr/local/share/sdcc/lib/medium -l mcs51 -l libsdcc -l libint -l liblong -l libfloat
LDLIBS = -k /usr/local/bin/../share/sdcc/lib/medium -k /usr/local/share/sdcc/lib/medium -l libsdcc -l libint -l liblong -l libfloat
LDFLAGS_SA = -muwx -b SSEG=0x80 $(LDLIBS_SA) -M -Y -b BSEG=6
LDFLAGS = -muwx -b SSEG=0x80 $(LDLIBS) -M -Y 
LDEVEN = -b GSINIT0=$(BASE0)
LDODD  = -b GSINIT0=$(BASE1)

APP_NAME=app

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

############## App starts here ############################################
#
#	To build a SUOTA app:
#
#	- create a new app build directory
#	- follow the instructions below for building a kernel for SUOTA
#	- copy this Makefile to the app build directory
#	- remove everything below "## Kernel starts here ##"
#	- edit APP_NAME
#	- put your app's object files in AOBJ
#	- edit THIS_VERSION to 1 (increment each time you make a downloadble build)
#	


#
# unified build	for example application - you must include app_integrated.rel
#
$(APP_NAME).ihx:	$(KOBJ) $(AOBJ) kernel.ihx app_integrated.rel
	$(LD) $(LDFLAGS_SA) -i $(APP_NAME).ihx $(KOBJ) $(AOBJ) app_integrated.rel

#
# downloadable SUOTA builds for example application - you must include app_hdr.rel and syms.rel and -f kernel.lk
#
$(APP_NAME)_even.suota:	$(AOBJ) kernel.lk syms.rel app_hdr.rel fixcrc
	$(LD) $(LDEVEN) $(LDFLAGS) -f kernel.lk -i $(APP_NAME)_even.ihx  $(AOBJ) syms.rel app_hdr.rel
	./fixcrc -v $(THIS_VERSION) <$(APP_NAME)_even.ihx >$(APP_NAME)_even.suota

$(APP_NAME)_odd.suota:	$(AOBJ) kernel.lk syms.rel app_hdr.rel fixcrc
	$(LD) $(LDODD) $(LDFLAGS) -f kernel.lk -i $(APP_NAME)_odd.ihx  $(AOBJ) syms.rel app_hdr.rel
	./fixcrc -v $(THIS_VERSION) <$(APP_NAME)_odd.ihx >$(APP_NAME)_odd.suota

#
# 	example application
#
$(APP_NAME).rel:	sample_app/app.c  include/interface.h 
	$(CC) $(CFLAGS) -c sample_app/app.c -o $(APP_NAME).rel


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
kernel.lk:	kernel.map kernel/map.pl
	perl kernel/map.pl <kernel.map >kernel.lk
#
#	build kernel - kernel.ihx is what you build into a board (we'll add somethign here to allow
#		later patching of MAC addresses)
#
kernel.ihx kernel.map:	$(KOBJ) dummy.rel
	$(LD)  $(LDFLAGS_SA) -l mcs51 -b PSEG=0 -b XSEG=0x100 -i kernel.ihx $(KOBJ) dummy.rel 


#
#	build kernel objects - leds.rel and kerys.rel should be left out of you don't need them
#
task.rel:	kernel/task.c  include/rf.h include/task.h include/interface.h  include/suota.h include/protocol.h
	$(CC) $(CFLAGS) -c kernel/task.c
rf.rel:	kernel/rf.c  include/rf.h include/task.h include/interface.h  include/suota.h include/protocol.h
	$(CC) $(CFLAGS) -c kernel/rf.c
keys.rel:	kernel/keys.c  include/rf.h include/task.h include/interface.h  include/suota.h include/protocol.h
	$(CC) $(CFLAGS) -c kernel/keys.c
suota.rel:	kernel/suota.c  include/rf.h include/task.h include/interface.h  include/suota.h include/protocol.h
	$(CC) $(CFLAGS) -c kernel/suota.c
suota_key.rel:	kernel/suota_key.c		#change this if you want a private efault suota key
	$(CC) $(CFLAGS) -c kernel/suota_key.c
leds.rel:	kernel/leds.s  
	$(AS)   -z -l -o leds.rel kernel/leds.s

#
#	this file provides access to kernel symbols exported using the linker .lk file - this
#		is solely to avoid a bug in sdcc's linker which complains if you have a -g
#		directive to declare a global variable but never use it
#
syms.rel:	kernel/syms.c  
	$(CC) $(CFLAGS) -c kernel/syms.c

#
#	code header for unified build
#
app_integrated.rel:	kernel/app_integrated.c  include/rf.h include/task.h include/interface.h  include/suota.h include/protocol.h
	$(CC) $(CFLAGS) -c kernel/app_integrated.c

#
#	code header for SUOTA apps
#
app_hdr.rel:	kernel/app_hdr.c  include/rf.h include/task.h include/interface.h  include/suota.h include/protocol.h
	$(CC) $(CFLAGS) -c kernel/app_hdr.c

#
#	dummy code header for kernel build
#
dummy.rel:	kernel/dummy.c  include/protocol.h
	$(CC) $(CFLAGS) -c kernel/dummy.c

#
#	app to fill in code headers in SUOTA builds and create loadable binaries
#
fixcrc:	kernel/fixcrc.c
	$(HOST_CC) -o fixcrc kernel/fixcrc.c


#
#	linux side packet interface code for talking to the serial internet gateway
#	plus example interactive version - this app will host a SUOTA server for your
#	network
#
packet_loader:	packet_interface.o packet_loader.o
	$(HOST_CPP) -o packet_loader packet_interface.o packet_loader.o -lpthread -g -lz
packet_loader.o:	serial/packet_interface.h serial/test.cpp
	$(HOST_CPP) -c serial/test.cpp -o packet_loader.o -I include -I serial -g 
packet_interface.o:	serial/packet_interface.h serial/packet_interface.cpp
	$(HOST_CPP) -c serial/packet_interface.cpp -I include -I serial -g


#
#
#	unified serial app for the dev board that turns it into a packet gateway
#
SOBJ = serial_app.rel app_integrated.rel
serial.ihx:	$(KOBJ) packet_interface.o $(SOBJ) 
	$(LD) $(LDFLAGS_SA) -i serial.ihx $(KOBJ) $(SOBJ)
serial_app.rel:	serial/serial_app.c  include/rf.h include/task.h include/interface.h  include/suota.h serial/packet_interface.h include/protocol.h
	$(CC) $(CFLAGS) -c serial/serial_app.c



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

suota_test:	test2_even.suota test3_odd.suota test4_even.suota test5_odd.suota
test2_even.suota:	test2.rel kernel.lk syms.rel app_hdr.rel fixcrc
	$(LD) $(LDEVEN) $(LDFLAGS) -f kernel.lk -i test2.ihx  test2.rel syms.rel app_hdr.rel
	./fixcrc -v 2 -k ccb17cb57448634ce595c15acf966145 <test2.ihx >test2_even.suota
test2.rel:	sample_app/app.c  include/interface.h 
	$(CC) $(CFLAGS) -c sample_app/app.c -o test2.rel -DVV="\"Test 2\n\""

test3_odd.suota:	test3.rel kernel.lk syms.rel app_hdr.rel fixcrc
	$(LD) $(LDODD) $(LDFLAGS) -f kernel.lk -i test3.ihx  test3.rel syms.rel app_hdr.rel
	./fixcrc -v 3 -k ccb17cb57448634ce595c15acf966145 <test3.ihx >test3_odd.suota
test3.rel:	sample_app/app.c  include/interface.h 
	$(CC) $(CFLAGS) -c sample_app/app.c -o test3.rel -DVV="\"Test 3\n\""

test4_even.suota:	test4.rel kernel.lk syms.rel app_hdr.rel fixcrc
	$(LD) $(LDEVEN) $(LDFLAGS) -f kernel.lk -i test4.ihx  test4.rel syms.rel app_hdr.rel
	./fixcrc -v 4 -k ccb17cb57448634ce595c15acf966145 <test4.ihx >test4_even.suota
test4.rel:	sample_app/app.c  include/interface.h 
	$(CC) $(CFLAGS) -c sample_app/app.c -o test4.rel -DVV="\"Test 4\n\""

test5_odd.suota:	test5.rel kernel.lk syms.rel app_hdr.rel fixcrc
	$(LD) $(LDODD) $(LDFLAGS) -f kernel.lk -i test5.ihx  test5.rel syms.rel app_hdr.rel
	./fixcrc -v 5 -k ccb17cb57448634ce595c15acf966145 <test5.ihx >test5_odd.suota
test5.rel:	sample_app/app.c  include/interface.h 
	$(CC) $(CFLAGS) -c sample_app/app.c -o test5.rel -DVV="\"Test 5\n\""

