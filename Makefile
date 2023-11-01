
#
# makefile for the javascript-to-C compiler
#
# this makefile assumes that the compiler 'gcc' and the
# library editor 'ar' can be found in the same directory
# as 'make'.  if not the case, fix GCCDIR variable below.
#

OBJDIR = .obj
GCCDIR = $(dir $(MAKE))

GCC = $(GCCDIR)gcc -municode -mconsole -std=c99 -O0 -I. -Iruntime \
    -Wall -pedantic -Wno-pedantic-ms-format -Wno-trigraphs \
    -D__USE_MINGW_ANSI_STDIO -o

RUNTIME = $(OBJDIR)/runtime.a
RUNTIME1 = $(OBJDIR)/runtime1.o
RUNTIME2 = $(OBJDIR)/runtime2.o

#
# compile javacript to EXE via a C compiler
#

all: FORCE
	@echo usage: make path/to/javascript/source.js
	@echo - will build executable as $(OBJDIR)/tmp
	@echo or, to run tests, type: make tests

%.js: $(RUNTIME) FORCE
	@node index.js $@ > $(OBJDIR)/tmp.c
	@$(GCC) $(OBJDIR)/tmp $(OBJDIR)/tmp.c $(RUNTIME)

#
# run compiler tests
#

tests: $(RUNTIME) FORCE
	@$(MAKE) -s test/suite/array.test
	@$(MAKE) -s test/suite/bigint.test
	@$(MAKE) -s test/suite/conditionals.test
	@$(MAKE) -s test/suite/declarations.test
	@$(MAKE) -s test/suite/destructuring.test
	@$(MAKE) -s test/suite/new_constructors.test
	@$(MAKE) -s test/suite/numeric_loops.test
	@$(MAKE) -s test/suite/object_expression.test
	@$(MAKE) -s test/suite/set_property_primitive.test
	@$(MAKE) -s test/suite/with_scoping.test

COMPRESS_SPACES = tr "[\n\r]" ["  "] | tr -s "[:space:]"

test/suite/%.test: FORCE
	echo Testing: $(@F:.test=)
	$(MAKE) $(@:.test=.js)
	$(OBJDIR)/tmp.exe > $(OBJDIR)/testExe1.txt
	node $(@:.test=.js) > $(OBJDIR)/testNode1.txt
	cat $(OBJDIR)/testExe1.txt  | $(COMPRESS_SPACES) > $(OBJDIR)/testExe2.txt
	cat $(OBJDIR)/testNode1.txt | $(COMPRESS_SPACES) > $(OBJDIR)/testNode2.txt
	cmp $(OBJDIR)/testNode2.txt $(OBJDIR)/testExe2.txt

test/%.js: $(RUNTIME) FORCE
	@node index.js $@ > $(OBJDIR)/tmp.c
	@$(GCC) $(OBJDIR)/tmp $(OBJDIR)/tmp.c $(RUNTIME)
	@/gcc/bin/objdump -M intel -d -s $(OBJDIR)/tmp.exe > $(OBJDIR)/tmp.asm

#
# build runtime in objdir
#

RUNTIME_DEPS_C = $(wildcard runtime/*.c)
RUNTIME_DEPS_H = $(wildcard runtime/*.h)
RUNTIME_DEPS_JS = $(wildcard runtime/js/*.js)

$(RUNTIME1): $(RUNTIME_DEPS_C) $(RUNTIME_DEPS_H)
	@mkdir -p $(OBJDIR)
	$(GCC) $(RUNTIME1) -c runtime/runtime.c

$(RUNTIME2): $(RUNTIME_DEPS_JS) $(RUNTIME1)
	$(GCC) $(OBJDIR)/runtime2.js -E -P -x c runtime/js/main.js
	echo '#define js_main js_init2' > $(OBJDIR)/runtime2.c
	node index.js $(OBJDIR)/runtime2.js >> $(OBJDIR)/runtime2.c
	$(GCC) $(RUNTIME2) -c $(OBJDIR)/runtime2.c

$(RUNTIME): $(RUNTIME1) $(RUNTIME2)
	$(GCCDIR)ar rcs $(RUNTIME) $(RUNTIME1) $(RUNTIME2)

runtime/js/%.js: FORCE
	@

#
# clean executables in objdir
#

backup/%.7z: FORCE
	echo Backing up: $(@:00=44)

clean: FORCE
	rm -rf $(OBJDIR)

FORCE:
