# Test for SMAA library

CCXX = g++
PROG = filter
TEXGEN=smaa_areatex
OBJS = filter.o smaa.o smaa_types.o
LIBS = -lm -lpng
RESULT = invader_result.png

$(PROG): $(OBJS)
	$(CCXX) -o $@ $^ $(LIBS)

.SUFFIXES: .o .cpp .cu .h

.cpp.o:
	$(CCXX) $(CFLAGS) $(INCLUDES) -c $< $(LIBS)

.PHONY: test clean check_syntax

clean:
	-rm -v $(OBJS) $(PROG)
	-rm -v $(TEXGEN) $(TEXGEN).h
	-rm -v tests/*_result.png

check-syntax:
	LANG=C $(CCXX) -Wall -fsyntax-only $(CHK_SOURCES)

$(OBJS): smaa.h smaa_types.h $(TEXGEN).h

$(TEXGEN).h: $(TEXGEN)
	./$(TEXGEN) $(TEXGEN).h

$(TEXGEN): $(TEXGEN).cpp
	$(CCXX) -o $(TEXGEN) $(CFLAGS) $(TEXGEN).cpp

test: $(PROG)
	for f in invader circle square monkey mizuki suzu pattern; do \
		./$(PROG) tests/$${f}.png tests/$${f}_result.png; \
		diff -s tests/$${f}_aa.png tests/$${f}_result.png; \
	done
