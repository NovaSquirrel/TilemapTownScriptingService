objlist := main luau luau_api
program_title = luatest

LUAU := ../luau-0.656

CC := g++
LD := g++

objdir := obj
srcdir := src
objlisto := $(foreach o,$(objlist),$(objdir)/$(o).o)

CFLAGS := -Wall -O2 -ggdb -I$(LUAU)/Compiler/include/ -I$(LUAU)/VM/include/
LDLIBS := -lm -L$(LUAU)/cmake -l:libLuau.Compiler.a -l:libLuau.Ast.a -l:libLuau.VM.a

luatest: $(objlisto)
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(objdir)/%.o: $(srcdir)/%.cpp
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean

clean:
	-rm $(objdir)/*.o
