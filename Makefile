all:	sys.hex 

CC = sdcc
LD = sdcc
AS = sdas8051

CFLAGS = -mmcs51 --model-medium --opt-code-size --debug -Iinclude
CFLAGS += -DKEYS
LDFLAGS = -mmcs51  --model-medium --opt-code-size --xram-size 3840 --code-size 20480 --debug



OBJ = task.rel suota.rel rf.rel leds.rel keys.rel 
TOBJ = test.rel  leds.rel

CFLAGS += -DAPP        
AOBJ = app.rel
sys.hex:	$(OBJ) $(AOBJ)
	$(LD) $(LDFLAGS) -o sys.hex $(OBJ) $(AOBJ)

test:	$(TOBJ)
	$(LD) $(LDFLAGS) -o test $(TOBJ)

task.rel:	kernel/task.c  include/rf.h include/task.h include/interface.h  include/suota.h 
	$(CC) $(CFLAGS) -c kernel/task.c
rf.rel:	kernel/rf.c  include/rf.h include/task.h include/interface.h  include/suota.h
	$(CC) $(CFLAGS) -c kernel/rf.c
keys.rel:	kernel/keys.c  include/rf.h include/task.h include/interface.h  include/suota.h
	$(CC) $(CFLAGS) -c kernel/keys.c
suota.rel:	kernel/suota.c  include/rf.h include/task.h include/interface.h  include/suota.h
	$(CC) $(CFLAGS) -c kernel/suota.c
test.rel:	test.c  
	$(CC) $(CFLAGS) -c test.c
leds.rel:	kernel/leds.s  
	$(AS)   -z -l -o leds.rel kernel/leds.s

app.rel:	sample_app/app.c  include/interface.h 
	$(CC) $(CFLAGS) -c sample_app/app.c
clean:
	rm -f *.rel *.map *.lst *.hex *.asm *.mem *.sym *.lk *.rst *.o *.cdb *.adb *.omf packet_test

packet_test:	packet_interface.o packet_test.o
	g++ -o packet_test packet_interface.o packet_test.o -lpthread -g


packet_test.o:	serial/packet_interface.h serial/test.cpp
	g++ -c serial/test.cpp -o packet_test.o -I include -I serial -g
packet_interface.o:	serial/packet_interface.h serial/packet_interface.cpp
	g++ -c serial/packet_interface.cpp -I include -I serial -g

serial_app.rel:	serial/serial_app.c  include/rf.h include/task.h include/interface.h  include/suota.h serial/packet_interface.h
	$(CC) $(CFLAGS) -c serial/serial_app.c
SOBJ = serial_app.rel
serial.hex:	$(OBJ) packet_interface.o $(SOBJ)
	$(LD) $(LDFLAGS) -o serial.hex $(OBJ) $(SOBJ)
