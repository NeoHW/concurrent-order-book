CC = clang
CXX = clang++

CFLAGS := $(CFLAGS) -g -O3 -Wall -Wextra -pedantic -Werror -std=c18 -pthread
CXXFLAGS := $(CXXFLAGS) -g -O3 -Wall -Wextra -pedantic -Werror -std=c++20 -pthread

ifeq ($(SANITIZER),asan)
    CFLAGS += -fsanitize=address
    CXXFLAGS += -fsanitize=address
    LDFLAGS += -fsanitize=address
endif

ifeq ($(SANITIZER),tsan)
    CFLAGS += -fsanitize=thread -fPIE
    CXXFLAGS += -fsanitize=thread -fPIE
    LDFLAGS += -fsanitize=thread -pie
endif

ifeq ($(SANITIZER),msan)
    CFLAGS += -fsanitize=memory
    CXXFLAGS += -fsanitize=memory
    LDFLAGS += -fsanitize=memory
endif


BUILDDIR = build

SRCS = main.cpp engine.cpp io.cpp

all: engine client

engine: $(SRCS:%=$(BUILDDIR)/%.o)
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

client: $(BUILDDIR)/client.cpp.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

.PHONY: clean
clean:
	rm -rf $(BUILDDIR)
	rm -f client engine

DEPFLAGS = -MT $@ -MMD -MP -MF $(BUILDDIR)/$<.d
COMPILE.cpp = $(CXX) $(DEPFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c

$(BUILDDIR)/%.cpp.o: %.cpp | $(BUILDDIR)
	$(COMPILE.cpp) $(OUTPUT_OPTION) $<

$(BUILDDIR): ; @mkdir -p $@

DEPFILES := $(SRCS:%=$(BUILDDIR)/%.d) $(BUILDDIR)/client.cpp.d

-include $(DEPFILES)
