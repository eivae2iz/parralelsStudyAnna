PROGPATH=../
INCLUDES=$(PROGPATH)/include
TOOLSPATH=$(PROGPATH)/tools
CORE=$(PROGPATH)/core
CFLAGS=-I$(INCLUDES) -O3 -g

all: test run_test

test: test.c $(PROGPATH)/faa.a
	$(CC) test.c $(CFLAGS) $(PROGPATH)/faa.a $(TOOLSPATH)/zlib/libz.a -o test
run_test:
	./test
$(PROGPATH)/faa.a: 
	$(MAKE) -C $(PROGPATH)
clean:
	rm *temp test
cleanall:
	$(MAKE) -C $(PROGPATH) cleanall

