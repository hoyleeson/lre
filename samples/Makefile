LDFLAGS := -ldl -lpthread ../liblre.a
#-L.. -llre
CFLAGS := -g -Ofast -Wall -fPIC -I..

SRCS := main.c  lrc_funcsample.c  lrc_callsample.c  lrc_samplez.c

OBJS := $(patsubst %.c,%.o,$(SRCS))

samples_bin := samples
lrtests_bin := lrtests

target := $(samples_bin) $(lrtests_bin)

all: $(target)

$(samples_bin): $(OBJS)
	$(CC) -g -o $@ $^ $(LDFLAGS)

$(lrtests_bin): lrtests.c
	$(CC) -g -o $@ $^ $(LDFLAGS)

%.o : %.c
	gcc $(CFLAGS) -c $< $(INCLUDES) -o $@

.depend: Makefile $(SRC)
	@$(CC) $(CFLAGS) -MM $(SRC) >$@
	sinclude .depend

clean:
	rm -fr *.o $(target)

