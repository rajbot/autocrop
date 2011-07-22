CXX=g++
CXXFLAGS=-ansi -D_BSD_SOURCE -DANSI -fPIC -O3 -DL_LITTLE_ENDIAN -I../leptonlib-1.56/src
#CXXFLAGS=-ansi -Werror -D_BSD_SOURCE -DANSI -fPIC -O3 -DL_LITTLE_ENDIAN
LDFLAGS=-ltiff -ljpeg -lpng -lz -lm
.PHONY=all clean
OBJ=cropAndSkewProxy.o cropAndSkewTwo.o
LIB=../leptonlib-1.56/lib/nodebug/liblept.a
BIN=cropAndSkewProxy cropAndSkewTwo

all : $(BIN)

cropAndSkewProxy : $(LIB) cropAndSkewProxy.o
	$(CXX) $(CXXFLAGS) -I/usr/X11R6/include cropAndSkewProxy.o $(LIB) $(LDFLAGS) -o $@


cropAndSkewTwo : $(LIB) cropAndSkewTwo.o
	$(CXX) $(CXXFLAGS) -I/usr/X11R6/include cropAndSkewTwo.o $(LIB) $(LDFLAGS) -o $@


%.o : %.c
	$(CXX) $(CXXFLAGS) -c $^ -o $@


clean :
	rm -vf *.o $(BIN)

