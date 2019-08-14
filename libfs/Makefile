# Target library
lib := libfs.a
objs := disk.o fs.o

CC := gcc
CFLAGS := -Wall -Werror
CFLAGS += -g
PANDOC := pandoc

ifneq ($(V),1)
Q = @
endif

all: $(lib)

deps := $(patsubst %.o,%.d,$(objs))
-include $(deps)
DEPFLAGS = -MMD -MF $(@:.o=.d)

libfs.a: $(objs)
	ar rcs libfs.a $(objs)

%.o: %.c
	$(Q)$(CC) $(CFLAGS) -c -o $@ $< $(DEPFLAGS)

clean:
	$(Q)rm -f $(targets) $(objs) $(deps)
