#自动编译
# 工具链根目录
TOOLCHAIN := toolchain/gcc-arm-11.2-2022.02-x86_64-aarch64-none-linux-gnu/bin
CROSS_COMPILE := $(TOOLCHAIN)/aarch64-none-linux-gnu-
CC := $(CROSS_COMPILE)gcc

# 第三方库目录
THIRD_PARTY ?= third_party

# 项目内 ALSA 安装目录（交叉编译产物）
ALSA_DIR ?= $(THIRD_PARTY)/alsa
ALSA_INC ?= $(ALSA_DIR)/include
ALSA_LIB ?= $(ALSA_DIR)/lib

JSON_DIR ?= $(THIRD_PARTY)/json-c
JSON_INC ?= $(JSON_DIR)/usr/include
JSON_LIB ?= $(JSON_DIR)/usr/lib

Target=smart_speaker
Object=main.o select.o link.o player.o socket.o device.o
CFLAGS=-Wall -I$(ALSA_INC) -I$(JSON_INC)
LDFLAGS=-L$(ALSA_LIB) -L$(JSON_LIB)
LIBS=-ljson-c -lpthread -lasound

all: $(Target)
$(Target): $(Object)
	$(CC) $(CFLAGS) -o $(Target) $(Object) $(LDFLAGS) $(LIBS)

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

select.o: select.c
	$(CC) $(CFLAGS) -c select.c

link.o: link.c
	$(CC) $(CFLAGS) -c link.c

player.o: player.c
	$(CC) $(CFLAGS) -c player.c

socket.o: socket.c
	$(CC) $(CFLAGS) -c socket.c

device.o: device.c
	$(CC) $(CFLAGS) -c device.c

clean:
	rm -f *.o $(Target)


