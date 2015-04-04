CC 		= gcc
CFLAGS 	= -std=gnu99 -lm  


#Source file list.
SOURCES_LOCAL =

SOURCES_LOCAL += ../../lib/common/common.c
SOURCES_LOCAL += ../../lib/network/fifo.c

SOURCES_LOCAL += ../../lib/network/stream.c
SOURCES_LOCAL += ../../lib/network/message.c
SOURCES_LOCAL += ../../lib/network/crc.c



SOURCES_LOCAL += ../../lib_ion/bowbus.c
SOURCES_LOCAL += ../../lib_ion/crc8.c
OBJECTS_LOCAL = $(SOURCES_LOCAL:.c=.o)

#Unique to each app.

SOURCES_APP1 = ionapp.c
SOURCES_APP1 += ../../lib/network/libdebug.c
SOURCES_APP1 += ../../lib/network/uart.c
SOURCES_APP1 += ../../lib/network/file.c
OBJECTS_APP1 = $(SOURCES_APP1:.c=.o) 
#OBJECTS_APP1 += $(SOURCES_DEBUG:.c=app1debug/.o)

SOURCES_APP2 = ionloader.c
SOURCES_APP2 += ../../lib/network/libdebug.c
SOURCES_APP2 += ../../lib/network/uart.c
SOURCES_APP2 += ../../lib/network/file.c
OBJECTS_APP2 = $(SOURCES_APP2:.c=.o)

SOURCES_ALL = SOURCES_LOCAL + SOURCES_APP1 + SOURCES_APP2 
OBJECTS_ALL = $(SOURCES_ALL:.c=.o)



#Application name.
APPNAME1 = ionapp
APPNAME2 = ionloader


#make all will compile using gcc


all: ionapp loader

source: build $(OBJECTS_LOCAL) 
	@echo =================================
	@echo !  Finished building library    !
	@echo =================================
	@echo 	

ionapp: 
	#-rm $(OBJECTS_APP1)	
	$(MAKE) build source $(OBJECTS_APP1)  link-msg	
	$(CC) -o $(APPNAME1) $(OBJECTS_COMMON)  $(OBJECTS_LOCAL) $(OBJECTS_DEBUG) $(OBJECTS_APP1)$(ARCH)  $(CFLAGS) $(LIBS) $(DEFS)
	@echo =================================
	@echo !          FINISHED   IONAPP    !
	@echo =================================
	@echo 	

loader: 
	#-rm $(OBJECTS_APP2)	
	$(MAKE) build source $(OBJECTS_APP2)  link-msg	
	$(CC) -o $(APPNAME2) $(OBJECTS_COMMON)  $(OBJECTS_LOCAL) $(OBJECTS_DEBUG) $(OBJECTS_APP2) $(ARCH)  $(CFLAGS) $(LIBS) $(DEFS)
	@echo =================================
	@echo !          FINISHED  LOADER     !
	@echo =================================
	@echo 	

debug:
	-rm  $(OBJECTS_DEBUG)
	@echo =================================
	@echo !     Buidling  Debug           !
	@echo =================================
	$(MAKE) $(OBJECTS_DEBUG)

build:	
	$(eval DEFS = -DFILEDEBUG=1)
	$(eval DEFS += -DUARTDEBUG=1)
	$(eval LIBS = )
	@echo 
	@echo =================================
	@echo !     COMPILING SOURCE FILES    !
	@echo =================================

link-msg:
	@echo =================================
	@echo !     LINKING OBJECT FILES      !
	@echo =================================

$(OBJECTS_LOCAL): %.o: %.c	
	$(CC) -c $(CFLAGS) $(DEFS) $(LIBS) $< -o $@ 

$(OBJECTS_DEBUG): %.o: %.c	
	$(CC) -c $(CFLAGS) $(DEFS) $(LIBS) $< -o $@ 

$(OBJECTS_APP2): %.o: %.c	
	$(CC) -c $(CFLAGS) $(DEFS) $(LIBS) $< -o $@ 

clean: 
	-rm  $(OBJECTS_COMMON)  $(OBJECTS_LOCAL)
	-rm -r */*.o
	-rm -r *.o
	-rm -r ../../lib/*/*.o
	-rm *.exe
