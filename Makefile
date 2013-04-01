CXX=g++
CXXFLAGS=-ansi -Werror -D_BSD_SOURCE -DANSI -fPIC -O3 -DL_LITTLE_ENDIAN -Ileptonica-1.68/src
LDFLAGS=-ltiff -ljpeg -lpng -lz -lm
.PHONY=all clean utils test
OBJ=autoCropScribe.o autoCropCommon.o
LIB=leptonica-1.68/lib/nodebug/liblept.a
BIN=autoCropScribe

all : $(BIN) utils


autoCropScribe : $(LIB) $(OBJ)
	$(CXX) $(CXXFLAGS) -I/usr/X11R6/include $(OBJ) $(LIB) $(LDFLAGS) -o $@


leptonica-1.68/lib/nodebug/liblept.a :
	-(cd leptonica-1.68/ && \
	  ./configure && \
	  $(MAKE) && \
	  cd src/ && \
	  $(MAKE) && \
	  $(MAKE) -f makefile.static && \
	  cd ../prog/ && \
	  $(MAKE) )


utils :
	-(cd tests && $(MAKE) )


%.o : %.c
	$(CXX) $(CXXFLAGS) -c $^ -o $@


clean :
	rm -vf *.o $(BIN) $(LIB)
	-(cd leptonica-1.68 && $(MAKE) clean && \
      cd ../tests && $(MAKE) clean)

test :
	-(cd tests && $(MAKE) test)
