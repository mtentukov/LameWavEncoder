#just call "make"

CC = gcc
LFLAGS = -pthread -Xlinker -Bstatic -lmp3lame -lm -Xlinker -Bdynamic
#CFLAGS = -g -Wall 
CFLAGS = -O2 -Wall 

PRGNAME = lameWav2mp3

MAKEDEPENDNAME = makedepend -Y -w20 -f Makefile -s
objlist=\
	tools.o\
        lameWav2mp3.o

$(PRGNAME): $(objlist)
	$(CC) -o $(PRGNAME) $(objlist) $(LFLAGS)

.c.o:
	$(CC) $(CFLAGS) -c -o $*.o $<

clean:
	rm -f $(objlist) $(PRGNAME)

dep: depend

depend:
	$(MAKEDEPENDNAME) '# ******** DEPENDENCIES: *********' $(objlist:.o=.c) $(PRGNAME).c

# ******** DEPENDENCIES: *********

tools.o: tools.h
tools.o: comdef.h
lameWav2mp3.o: tools.h
lameWav2mp3.o: comdef.h
lameWav2mp3.o: queue.h
