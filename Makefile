CFLAGS+=-std=gnu99 -W -Wall -g


sources=main send

agim: $(addsuffix .o,$(sources))
	@echo LD $@
	@gcc -o $@ $^ $(LDFLAGS)

%.d: %.c
	@set -e; rm -f $@; \
		gcc -MM $< | \
		sed -e 's,\($*\)\.o[ :]*,\1.o $@ : ,g' > $@;

%.o: %.c
	@echo CC $<
	@gcc $(CFLAGS) -c $< -o $@

clean:
	@echo CLEAN
	@rm -f *.o *.d agim

-include $(addsuffix .d,$(sources))
