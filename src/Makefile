CXX = g++ -fPIC
CC = gcc
NETLIBS= -lnsl -lpthread -ldl
BUILDPATH = ../build
BINPATH = ../bin
MODBIN_PATH = ../http-root-dir/cgi-bin/

_MYHTTPD_OBJS = myhttpd.o send-response.o
MYHTTPD_OBJS = $(patsubst %.o,$(BUILDPATH)/%.o,$(_MYHTTPD_OBJS))

all: myhttpd

$(BUILDPATH)/%.o: %.cpp
	@echo 'Building $@ from $<'
	$(CXX) -o $@ -c -I. $<

myhttpd: $(MYHTTPD_OBJS)
	$(CXX) -o $(BINPATH)/$@ $^ $(NETLIBS)

clean:
	rm -f *.o hello.so
	rm -f *.o 



