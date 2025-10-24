# Makefile
override CXX := g++-14
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -g -fmodules-ts -MMD -MP

# specify directories and target
SRCDIR   := src
BUILDDIR := build
OBJDIR   := $(BUILDDIR)/obj
BIN      := $(BUILDDIR)/main

# source and object files
SRCS        := $(wildcard $(SRCDIR)/*.cpp)
MOD_CPPM    := $(wildcard $(SRCDIR)/*.cppm)
MOD_IXX     := $(wildcard $(SRCDIR)/*.ixx)

# 
OBJ_CPP     := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))
OBJ_MOD_CPPM:= $(patsubst $(SRCDIR)/%.cppm,$(OBJDIR)/%.o,$(MOD_CPPM))
OBJ_MOD_IXX := $(patsubst $(SRCDIR)/%.ixx,$(OBJDIR)/%.o,$(MOD_IXX))
OBJ_MODS    := $(OBJ_MOD_CPPM) $(OBJ_MOD_IXX)
OBJS        := $(OBJ_MODS) $(OBJ_CPP)
DEPS        := $(OBJS:.o=.d)

.PHONY: all run clean rebuild
all: $(BIN)

# Link
$(BIN): $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

# build .cpp files only after modules are built
$(OBJ_CPP): $(OBJ_MODS)

## special dependencies
# gtfs imports utility  -> utility must be compiled first.
$(OBJDIR)/gtfs_parser.o: $(OBJDIR)/utility.o
$(OBJDIR)/gtfs_parser.o: $(OBJDIR)/rdf_schema_templ.o
$(OBJDIR)/*_schema.o: $(OBJDIR)/rdf_schema_templ.o

# modules 
$(OBJDIR)/%.o: $(SRCDIR)/%.cppm
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -x c++ -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.ixx
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -x c++ -c $< -o $@

# 
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -c $< -o $@

# 
run: $(BIN)
	./$(BIN)

clean:
	rm -rf $(BUILDDIR) gcm.cache

rebuild: clean all

# ?
-include $(DEPS)
