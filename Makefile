TARGET  = maptest
APPNAME = "High High"
APPID   = "ZAMA"
DBTYPE  = "appl"

CC      = m68k-palmos-gcc -palmos5
CFLAGS  = -g -O2 -Wall -DUSE_GRAPH2

high.prc: high bin.res
	build-prc high.prc ${APPNAME} ${APPID} high *.bin

bin.res: high.rcp
	-rm -f *.bin
	pilrc -allowEditID -noEllipsis -Fj high.rcp .
	touch bin.res

graph.o: graph.c graph.h
graph2.o: graph2.c graph2.h glue.h
glue.o: glue.h

high: high.o graph2.o graph2.h glue.o high.h
	${CC} ${CFLAGS} high.o graph2.o glue.o -o high

clean:
	-rm -f *.o high *.bin *.stamp *.[pg]rc *~ bin.res
