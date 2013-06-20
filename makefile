#
# Marc DelaCruz
# CS 6378-5U1 AOS Project 1 Summer 2010 
# 


CFLAGS = -g -Wall

CC = g++
LIBS =  -lrt -lsocket -lnsl
INCLUDES =
OBJS = ClientSocket.o Process.o Fd.o ConnSocket.o ProcessArg.o TimedAction.o Message.o ServerSocket.o TimedActionHandler.o
SRCS = ClientSocket.cpp Process.cpp Fd.cpp ConnSocket.cpp ProcessArg.cpp TimedAction.cpp Message.cpp ServerSocket.cpp TimedActionHandler.cpp

HDRS = CallBack.h Process.h Fd.h ClientSocket.h ProcessArg.h TimedAction.h\
       ConnSocket.h ReadMessage.h TimedActionHandler.h Message.h ServerSocket.h\
       MessageDefs.h Singleton.h

all: skeen

skeen: SkeenProcess.o ${OBJS}
	${CC} ${CFLAGS} ${INCLUDES} -o skeen SkeenProcess.o ${OBJS} ${LIBS}

SkeenProcess.o: ${SRCS} SkeenProcess.h ${HDRS}
	${CC} ${CFLAGS} ${INCLUDES} -c SkeenProcess.cpp

Fd.o: ${SRCS} ${HDRS}
	${CC} ${CFLAGS} ${INCLUDES} -c Fd.cpp

ServerSocket.o: ${SRCS} ${HDRS}
	${CC} ${CFLAGS} ${INCLUDES} -c ServerSocket.cpp

ConnSocket.o: ${SRCS} ${HDRS}
	${CC} ${CFLAGS} ${INCLUDES} -c ConnSocket.cpp

ClientSocket.o: ${SRCS} ${HDRS}
	${CC} ${CFLAGS} ${INCLUDES} -c ClientSocket.cpp

Process.o: ${SRCS} ${HDRS}
	${CC} ${CFLAGS} ${INCLUDES} -c Process.cpp

ProcessArg.o: ${SRCS} ${HDRS}
	${CC} ${CFLAGS} ${INCLUDES} -c ProcessArg.cpp

TimedActionHandler.o: ${SRCS} ${HDRS}
	${CC} ${CFLAGS} ${INCLUDES} -c TimedActionHandler.cpp

TimedAction.o: ${SRCS} ${HDRS}
	${CC} ${CFLAGS} ${INCLUDES} -c TimedAction.cpp

Message.o: ${SRCS} ${HDRS}
	${CC} ${CFLAGS} ${INCLUDES} -c Message.cpp


clean:
	rm *.o skeen

#