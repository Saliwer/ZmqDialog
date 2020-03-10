THIS_DIR	= $(shell pwd)
PRJ_DIR         = $(THIS_DIR)/projects
BIN_DIR         = $(THIS_DIR)/bin
LIB_DIR         = $(THIS_DIR)/lib

CXX		= @g++
LD		= g++

DEBUGFLAG	= -g
#-O2
STDLIBS		= -lpthread -lreadline -lzmq

INCFLAGS	= -I. -I$(THIS_DIR)/include
DEFFLAGS	= 

CXXFLAGS	= $(DEBUGFLAG) -Wall -O -fexceptions $(INCFLAGS) $(DEFFLAGS) -Wno-deprecated -fPIC -std=c++11

LIB_OBJS	= obj/Exception.o obj/Debug.o obj/DlgMessage.o obj/DlgServer.o obj/DlgPublisher.o obj/DlgSubscriber.o

HEADERS		= $(wildcard include/*.h)

LIBS		= $(STDLIBS) -L$(LIB_DIR) -lZmqDlg

default:	obj lib $(LIB_DIR)/ZmqDlgLib

all:		

show:		
		@echo $(LIB_OBJS) $(HEADERS) $(THIS_DIR)

sources/depend.mk:	$(HEADERS)
		@echo "Create dependencies file"
		$(CXX) $(CXXFLAGS) sources/*.cc  -MM > sources/depend.mk

obj/%.o:          sources/%.cc sources/depend.mk
		@echo "Compilation: "$<
		$(CXX) $(CXXFLAGS) $(DEFFLAGS) -c $< -o $@
		@echo "$@ done..."

clean:
		@echo $(WD) "Cleaning ..."
		@rm -f obj/*  sources/depend.mk ./*~ sources/*~ lib/* bin/* include/*~ projects/*~

-include depend.mk

#$(BIN_DIR)/tcpost_server:	$(PRJ_DIR)/tcpost_server.cpp $(HEADERS) $(LIB_DIR)/TCPostLib
#		$(CXX) -o $@ $(CXXFLAGS) $(DEFFLAGS) $(PRJ_DIR)/tcpost_server.cpp $(STDLIBS) $(LIBS)
#		@echo "$@ done..."


#$(BIN_DIR)/tcpost_client:	$(PRJ_DIR)/tcpost_client.cpp $(HEADERS) $(LIB_DIR)/TCPostLib
#		$(CXX) -o $@ $(CXXFLAGS) $(DEFFLAGS) $(PRJ_DIR)/tcpost_client.cpp $(STDLIBS) $(LIBS)
#		@echo "$@ done..."


$(LIB_DIR)/ZmqDlgLib:	$(LIB_DIR)/libZmqDlg.so $(LIB_DIR)/libZmqDlg.a
		@echo "$@ done!"

$(LIB_DIR)/libZmqDlg.a: $(LIB_OBJS)	
		@ar rcs $@ $^
		@echo "$@ done!"

$(LIB_DIR)/libZmqDlg.so:	$(LIB_OBJS)
		${CXX} -shared -o $@ $^
		@echo "$@ done!"


obj:		
		@mkdir -p $(THIS_DIR)/obj

lib:		
		@mkdir -p $(LIB_DIR)
