# ===================== Makefile =====================
# Make 'all' the default goal
.DEFAULT_GOAL := all

# Compiler & flags
override CXX := g++-14
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -g -fmodules-ts -MMD -MP
CPPFLAGS :=
# Uncomment to work around rare GCC modules ICEs:
# CXXFLAGS += -fno-ipa-icf

# # --- Linker/runtime: use GCC-14's libstdc++ at run-time too ---
# STDCPP_DIR := $(dir $(shell $(CXX) -print-file-name=libstdc++.so.6))
# LDFLAGS   += -L$(STDCPP_DIR) -Wl,-rpath,$(STDCPP_DIR)

# Optional: make a self-contained binary
# LDLIBS += -static-libstdc++ -static-libgcc

# Layout
SRCDIR     := src
UTILDIR    := $(SRCDIR)/util
SCHEMADIR  := $(SRCDIR)/schema
BUILDDIR   := build
OBJDIR     := $(BUILDDIR)/obj
BINARY     := $(BUILDDIR)/main

# -------- Discover sources --------
UTIL_CPPM        := $(wildcard $(UTILDIR)/*.cppm) $(wildcard $(UTILDIR)/*.ixx)
SCHEMA_CORE_CPPM := $(SRCDIR)/schema_core.cppm
SCHEMA_CPPM      := $(wildcard $(SCHEMADIR)/*.cppm) $(wildcard $(SCHEMADIR)/*.ixx)
ROOT_CPPM        := $(filter-out $(SCHEMA_CORE_CPPM) $(REGISTRY_CPPM), \
                     $(wildcard $(SRCDIR)/*.cppm) $(wildcard $(SRCDIR)/*.ixx))
CPP_SRCS         := $(wildcard $(SRCDIR)/*.cpp)

# -------- Map to object files (preserve subdirs) --------
to_obj = $(patsubst $(SRCDIR)/%,$(OBJDIR)/%,$(1))
UTIL_OBJS        := $(call to_obj,$(UTIL_CPPM:.cppm=.o))
UTIL_OBJS        := $(UTIL_OBJS:.ixx=.o)
SCHEMA_CORE_OBJ  := $(call to_obj,$(SCHEMA_CORE_CPPM:.cppm=.o))
SCHEMA_OBJS      := $(call to_obj,$(SCHEMA_CPPM:.cppm=.o))
SCHEMA_OBJS      := $(SCHEMA_OBJS:.ixx=.o)
REGISTRY_OBJ     := $(call to_obj,$(REGISTRY_CPPM:.cppm=.o))
ROOT_OBJS        := $(call to_obj,$(ROOT_CPPM:.cppm=.o))
ROOT_OBJS        := $(ROOT_OBJS:.ixx=.o)
CPP_OBJS         := $(call to_obj,$(CPP_SRCS:.cpp=.o))

OBJS := $(UTIL_OBJS) $(SCHEMA_CORE_OBJ) $(SCHEMA_OBJS) $(REGISTRY_OBJ) $(ROOT_OBJS) $(CPP_OBJS)
DEPS := $(OBJS:.o=.d)

# -------- Auto module dependency discovery --------
SOURCES  := $(UTIL_CPPM) $(SCHEMA_CORE_CPPM) $(SCHEMA_CPPM) $(REGISTRY_CPPM) $(ROOT_CPPM) $(CPP_SRCS)
MODINDEX := $(BUILDDIR)/modindex.txt   # "module.name  build/obj/.../file.o"
MODDEPS  := $(BUILDDIR)/moddeps.mk     # generated object->object prerequisites

# Build index: module name -> object file (exporters)
$(MODINDEX): $(SOURCES)
	@mkdir -p $(dir $@)
	@echo "# auto-generated module index" > $@
	@for f in $^ ; do \
	  mod=$$(sed -n -E 's/^[[:space:]]*export[[:space:]]+module[[:space:]]+([[:alnum:]_.]+)[[:space:]]*;.*/\1/p' $$f | head -1); \
	  if [ -n "$$mod" ]; then \
	    obj="$(OBJDIR)/$${f#$(SRCDIR)/}"; obj="$${obj%.cppm}.o"; obj="$${obj%.ixx}.o"; obj="$${obj%.cpp}.o"; \
	    echo "$$mod $$obj" >> $@; \
	  fi; \
	done

# Build object->object deps from imports
$(MODDEPS): $(SOURCES) $(MODINDEX)
	@mkdir -p $(dir $@)
	@echo "# auto-generated module deps" > $@
	@for f in $(SOURCES); do \
	  obj="$(OBJDIR)/$${f#$(SRCDIR)/}"; obj="$${obj%.cppm}.o"; obj="$${obj%.ixx}.o"; obj="$${obj%.cpp}.o"; \
	  imps=$$(sed -n -E 's/^[[:space:]]*import[[:space:]]+([[:alnum:]_.]+)[[:space:]]*;.*/\1/p' $$f); \
	  deps=""; \
	  for m in $$imps; do \
	    dep=$$(awk -v k="$$m" '$$1==k{print $$2}' $(MODINDEX)); \
	    [ -n "$$dep" ] && deps="$$deps $$dep"; \
	  done; \
	  [ -n "$$deps" ] && echo "$$obj: $$deps" >> $@ || true; \
	done

# Include generated deps (safe if missing on first run)
-include $(MODDEPS)

# -------- Targets --------
.PHONY: all clean rebuild run
all: $(MODDEPS) $(BINARY)

# Link
$(BINARY): $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

# Compile rules
$(OBJDIR)/%.o: $(SRCDIR)/%.cppm
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -x c++ -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.ixx
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -x c++ -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -I$(SRCDIR) -c $< -o $@

# Convenience
run: $(BINARY)
	./$(BINARY)

clean:
	rm -rf $(BUILDDIR) gcm.cache

rebuild: clean all

# Auto-included header deps from -MMD -MP
-include $(DEPS)
# ===================== /Makefile =====================
