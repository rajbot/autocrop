CXX=g++
override CXXFLAGS+=-ansi -Werror -D_BSD_SOURCE -DANSI -fPIC -O3 -DL_LITTLE_ENDIAN -I../leptonica-1.68/src
LDFLAGS=-ltiff -ljpeg -lpng -lz -lm
.PHONY=all clean test
OBJ=cropAndSkewProxy.o cropAndSkewTwo.o
LIB=../leptonica-1.68/lib/nodebug/liblept.a
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

test : all
	mkdir -p debug-images/
	mkdir -p testrun/`date  '+%Y-%m-%d'`
	-(./processTestImages.py && \
    ./processFoldoutImages.py)
