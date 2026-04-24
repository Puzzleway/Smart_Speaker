Target=main
Object=main.o select.o player.o link.o socket.o device.o

#CFLAGS=-Wall -I/root/alsa-arm-install/include -I/root/json-arm-install/include
# Wall是编译器警告选项，-I是添加头文件路径
CFLAGS=-Wall

#LIBS=-ljson -lasound -L/root/alsa-arm-install/lib -L/root/json-arm-install/lib
LIBS=-ljson-c -lasound -lpthread

CC=gcc

$(Target):$(Object)
	$(CC) $(Object) -o $(Target) $(CFLAGS) $(LIBS)

clean:
	rm -f *.o main
