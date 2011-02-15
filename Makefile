CFLAGS+=-std=gnu99 -W -Wall -g
LDFLAGS+=-liw


sources = main process net wifi comb
files = $(addsuffix .c,$(sources)) main.h Makefile README INSTALL COPYING


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


VERSION = $(shell git describe --match='v[0-9]*' | tail -c +2)

src-pkg:
	@echo SPKG $(VERSION)
	@mkdir -p dist/agim-$(VERSION)
	@cp $(files) dist/agim-$(VERSION)
	@tar -czf dist/agim-$(VERSION)-src.tar.gz -C dist agim-$(VERSION)
	@rm -rf dist/agim-$(VERSION)

-include $(addsuffix .d,$(sources))
