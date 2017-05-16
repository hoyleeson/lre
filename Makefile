LDFLAGS := -ldl -lpthread
CFLAGS := -g -Ofast -Wall -fPIC

SRCS := lre.c  interpreter.c  syntax.c  lexer.c  semantic.c  execute.c  log.c  utils.c  vector.c
include lrc/Build.mk

OBJS := $(patsubst %.c,%.o,$(SRCS))

liblre_a := liblre.a
liblre_so := liblre.so

target := $(liblre_so) $(liblre_a)

all: $(target)
	make -C samples

$(liblre_so): $(OBJS)
	$(CC) -g -shared -o $@ $^ $(LDFLAGS)

$(liblre_a): $(OBJS)
	ar -rs $@ $^

%.o : %.c
	gcc $(CFLAGS) -c $< $(INCLUDES) -o $@

.depend: Makefile $(SRC)
	@$(CC) $(CFLAGS) -MM $(SRC) >$@
	sinclude .depend

clean:
	rm -fr *.o lrc/*o $(target)
	make -C samples clean

