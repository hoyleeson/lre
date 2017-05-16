LDFLAGS := -ldl -lpthread ../liblre.a
#-L.. -llre
CFLAGS := -g -Ofast -Wall -fPIC -I..

SRCS := main.c  lrc_yarascan.c

OBJS := $(patsubst %.c,%.o,$(SRCS))

samples_bin := samples

target := $(samples_bin)

all: $(target)

$(samples_bin): $(OBJS)
	$(CC) -g -o $@ $^ $(LDFLAGS)

%.o : %.c
	gcc $(CFLAGS) -c $< $(INCLUDES) -o $@

.depend: Makefile $(SRC)
	@$(CC) $(CFLAGS) -MM $(SRC) >$@
	sinclude .depend

clean:
	rm -fr *.o $(target)

