all:	sys.hex serial.hex

CC = sdcc
LD = sdcc
AS = sdas8051

CFLAGS = -mmcs51 --model-medium --opt-code-size --debug
CFLAGS += -DKEYS
LDFLAGS = -mmcs51  --model-medium --opt-code-size --xram-size 3840 --code-size 8192 --debug



OBJ = task.rel suota.rel rf.rel leds.rel keys.rel 
TOBJ = test.rel  leds.rel

CFLAGS += -DAPP        
AOBJ = app.rel
sys.hex:	$(OBJ) $(AOBJ)
	$(LD) $(LDFLAGS) -o sys.hex $(OBJ) $(AOBJ)

test:	$(TOBJ)
	$(LD) $(LDFLAGS) -o test $(TOBJ)

task.rel:	task.c  rf.h task.h interface.h  suota.h 
	$(CC) $(CFLAGS) -c task.c
rf.rel:	rf.c  rf.h task.h interface.h  suota.h
	$(CC) $(CFLAGS) -c rf.c
keys.rel:	keys.c  rf.h task.h interface.h  suota.h
	$(CC) $(CFLAGS) -c keys.c
serial_app.rel:	serial_app.c  rf.h task.h interface.h  suota.h
	$(CC) $(CFLAGS) -c serial_app.c
suota.rel:	suota.c  rf.h task.h interface.h  suota.h
	$(CC) $(CFLAGS) -c suota.c
test.rel:	test.c  
	$(CC) $(CFLAGS) -c test.c
leds.rel:	leds.s  
	$(AS)   -z -l -o leds.rel leds.s

app.rel:	app.c  interface.h 
	$(CC) $(CFLAGS) -c app.c
clean:
	rm -f *.rel *.map *.lst *.hex *.asm *.mem *.sym *.lk *.rst *.o *.cdb *.adb *.omf

packet_interface.o:	packet_interface.h packet_interface.cpp
	g++ -c packet_interface.cpp

SOBJ = serial_app.rel
serial.hex:	$(OBJ) packet_interface.o $(SOBJ)
	$(LD) $(LDFLAGS) -o serial.hex $(OBJ) $(SOBJ)

s.rel: s.c 
	$(CC) $(CFLAGS) -c s.c
s.hex:	s.rel
	$(LD) $(LDFLAGS) -o s.hex s.rel

