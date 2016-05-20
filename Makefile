CC = g++ 
CFLAGS = -Wall 
LIBS = -lpthread 
TARGET = raw_test 
RM = rm -f 
OBJS = client_handle.o main.o parameters.o server.o worker.o demultiplex.o timer.o
all:$(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIBS)
clean:
	$(RM) $(TARGET) $(OBJS)
