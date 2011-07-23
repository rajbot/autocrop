CXX=g++
CXXFLAGS=-ansi -Werror -D_BSD_SOURCE -DANSI -fPIC -O3 -DL_LITTLE_ENDIAN -Ileptonlib-1.56/src
LDFLAGS=-ltiff -ljpeg -lpng -lz -lm
.PHONY=all clean leptonlib utils test
OBJ=autoCropScribe.o autoCropCommon.o
LIB=leptonlib-1.56/lib/nodebug/liblept.a
BIN=autoCropScribe

all : $(BIN) utils


autoCropScribe : $(LIB) $(OBJ)
	$(CXX) $(CXXFLAGS) -I/usr/X11R6/include $(OBJ) $(LIB) $(LDFLAGS) -o $@


leptonlib-1.56/lib/nodebug/liblept.a : 
	-(cd leptonlib-1.56/ && \
	  ./configure && \
	  $(MAKE) && \
	  cd src/ && \
	  $(MAKE) && \
	  cd ../prog/ && \
	  $(MAKE) )


utils :
	-(cd tests && $(MAKE) )


%.o : %.c
	$(CXX) $(CXXFLAGS) -c $^ -o $@


clean :
	rm -vf *.o $(BIN) $(LIB) \ 
	-(cd leptonlib-1.56 && $(MAKE) clean)
	-(cd tests && $(MAKE) clean)


test :
	-(cd tests && $(MAKE) test)
