LDFLAGS := -ldl -lpthread
CFLAGS := -g -Ofast -Wall -fPIC

SRCS := lre.c  mempool.c  conf.c  calculate.c  keyword.c  interpreter.c  macro.c  preprocess.c  syntax.c  lexer.c  semantic.c  execute.c  \
	log.c  utils.c  vector.c

include lrc/Build.mk

OBJS := $(patsubst %.c,%.o,$(SRCS))

liblre_a := liblre.a
liblre_so := liblre.so

lrexe_bin := lrexe

target := $(liblre_so) $(liblre_a) $(lrexe_bin)

all: $(target)
	make -C samples

$(liblre_so): $(OBJS)
	$(CC) -g -shared -o $@ $^ $(LDFLAGS)

$(liblre_a): $(OBJS)
	ar -rs $@ $^

$(lrexe_bin): lrexe.c
	$(CC) -g -o $@ $^ $(LDFLAGS) $(liblre_a)

%.o : %.c
	gcc $(CFLAGS) -c $< $(INCLUDES) -o $@

.depend: Makefile $(SRC)
	@$(CC) $(CFLAGS) -MM $(SRC) >$@
	sinclude .depend

clean:
	rm -fr *.o lrc/*o $(target)
	make -C samples clean

