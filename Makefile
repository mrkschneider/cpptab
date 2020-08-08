include Makefile.env
INCLUDEDIRS=./include $(BOOSTDIR)/include $(CIRCBUFDIR)/include $(LINESCANDIR)/include $(CLI11DIR)/include
LIBDIRS=$(OUTLIBDIR) $(BOOSTDIR)/lib $(CIRCBUFDIR)/lib $(LINESCANDIR)/lib
LIBNAMES=circbuf.opt linescan :libboost_regex.a
TESTINCLUDEDIRS=$(INCLUDEDIRS) $(CXXTESTDIR)
OUTLIBDIR=lib
TESTLIBDIRS=$(LIBDIRS)
SRCDIR=./src
TESTSRCDIR=./test
TESTBIN=testrunner
OUTLIBNAME=csv
TESTLIBNAMES=$(OUTLIBNAME) $(LIBNAMES)
OUTLIB=$(OUTLIBDIR)/lib$(OUTLIBNAME).a
OUTBIN=main.out

CXX=g++
CXXFLAGS=-Wall -Wpedantic -std=c++17 -O3 -g3 
LDFLAGS=

OBJDIR=objects
SOURCES=$(shell find $(SRCDIR) -type f -name "*.cpp")
TESTSOURCES=$(shell find $(TESTSRCDIR) -type f -name "*test.hpp")

OBJECTS=$(addprefix $(OBJDIR)/,$(SOURCES:.cpp=.o))
INCLUDELINE=$(addprefix -I,$(INCLUDEDIRS))
LIBLINE=$(addprefix -L,$(LIBDIRS))
TESTINCLUDELINE=$(addprefix -I,$(TESTINCLUDEDIRS))
TESTLIBLINE=$(addprefix -L,$(TESTLIBDIRS))
LIBNAMELINE=$(addprefix -l,$(LIBNAMES))
TESTLIBNAMELINE=$(addprefix -l,$(TESTLIBNAMES))
SHELL=/bin/bash

$(OUTBIN): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBLINE) $(LIBNAMELINE)

lib: $(OUTLIB)

$(OUTLIB): $(OBJECTS)
	ar rcs $@ $^

test: lib
	$(CXXTESTDIR)/bin/cxxtestgen --error-printer -o $(TESTBIN).cpp $(TESTSOURCES)
	$(CXX) $(CXXFLAGS) $(TESTINCLUDES) $(TESTLIBDIRS) $(LDFLAGS) -o $(TESTBIN) $(TESTBIN).cpp $(TESTLIBLINE) $(TESTLIBNAMELINE)
	rm -f $(TESTBIN).cpp
	./$(TESTBIN)

$(OBJECTS): $(OBJDIR)/%.o: %.cpp
	@[ -d $@ ] || mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDELINE) -c -o $@ $<

clean:
	rm -rfv $(OBJDIR)
	rm -fv $(OUTLIB)
	rm -fv $(OUTBIN)

