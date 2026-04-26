Target=main
Object=main.o server.o database.o player.o

CXXFLAGS = -std=c++11 -g -I/usr/include/jsoncpp
LDFLAGS  = -levent -lmysqlclient -ljsoncpp

$(Target):$(Object)
	g++ $(Object) -o $(Target) $(LDFLAGS)

%.o: %.cpp
	g++ $(CXXFLAGS) -c $< -o $@

clean:
	rm $(Object) $(Target)
