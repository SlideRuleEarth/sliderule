##############################################################################
## ATLAS Sigview Application Definitions

ROOT = $(shell pwd)/../..

VPATH := $(ROOT)/platforms/linux
VPATH += $(ROOT)/packages/core
VPATH += $(ROOT)/packages/ccsds
VPATH += $(ROOT)/packages/legacy
VPATH += $(ROOT)/plugins/sigview
VPATH += $(ROOT)/targets/sliderule-sigview

INCLUDES := -I$(ROOT)/platforms/linux
INCLUDES += -I$(ROOT)/packages/core
INCLUDES += -I$(ROOT)/packages/ccsds
INCLUDES += -I$(ROOT)/packages/legacy
INCLUDES += -I$(ROOT)/plugins/sigview
INCLUDES += -I$(ROOT)/targets/sliderule-sigview

# SigView Application #
APP_OBJ := SigView.o
APP_OBJ += Viewer.o
APP_OBJ += Shell.o
APP_OBJ += Charter.o

# Sigview Plugin #
APP_OBJ += sigview.o
APP_OBJ += AdasSocketReader.o
APP_OBJ += ItosRecord.o
APP_OBJ += ItosRecordParser.o
APP_OBJ += DatasrvSocketReader.o
APP_OBJ += AtlasFileWriter.o
APP_OBJ += AtlasHistogram.o
APP_OBJ += AltimetryHistogram.o
APP_OBJ += TimeTagHistogram.o
APP_OBJ += BceHistogram.o
APP_OBJ += TimeTagProcessorModule.o
APP_OBJ += AltimetryProcessorModule.o
APP_OBJ += MajorFrameProcessorModule.o
APP_OBJ += TimeProcessorModule.o
APP_OBJ += LaserProcessorModule.o
APP_OBJ += BceProcessorModule.o
APP_OBJ += CmdEchoProcessorModule.o
APP_OBJ += DiagLogProcessorModule.o
APP_OBJ += ReportProcessorStatistic.o
APP_OBJ += HstvsSimulator.o
APP_OBJ += BlinkProcessorModule.o
APP_OBJ += TxTimeProcessor.o

# Legacy Package #
APP_OBJ += legacy.o
APP_OBJ += CcsdsFileWriter.o
APP_OBJ += CcsdsFrameStripper.o
APP_OBJ += CcsdsMsgProcessor.o
APP_OBJ += CcsdsPacketProcessor.o
APP_OBJ += CcsdsProcessorModule.o
APP_OBJ += CcsdsPublisherProcessorModule.o
APP_OBJ += CcsdsRecordFileWriter.o
APP_OBJ += CfsInterface.o
APP_OBJ += CommandableObject.o
APP_OBJ += CommandProcessor.o
APP_OBJ += CosmosInterface.o
APP_OBJ += LuaInterpreter.o
APP_OBJ += LuaLibraryCmd.o
APP_OBJ += UT_Dictionary.o
APP_OBJ += UT_MsgQ.o
APP_OBJ += UT_Table.o
APP_OBJ += UT_TimeLib.o
	
# CCSDS Package #
APP_OBJ += ccsds.o
APP_OBJ += CcsdsPacket.o
APP_OBJ += CcsdsPacketizer.o
APP_OBJ += CcsdsPacketInterleaver.o
APP_OBJ += CcsdsPacketParser.o
APP_OBJ += CcsdsParserAOSFrameModule.o
APP_OBJ += CcsdsParserModule.o
APP_OBJ += CcsdsParserStripModule.o
APP_OBJ += CcsdsParserZFrameModule.o
APP_OBJ += CcsdsPayloadDispatch.o
APP_OBJ += CcsdsRecord.o
APP_OBJ += CcsdsRecordDispatcher.o

# Core Package #
APP_OBJ += core.o
APP_OBJ += Asset.o
APP_OBJ += CaptureDispatch.o
APP_OBJ += ClusterSocket.o
APP_OBJ += CsvDispatch.o
APP_OBJ += DeviceIO.o
APP_OBJ += DeviceObject.o
APP_OBJ += DeviceReader.o
APP_OBJ += DeviceWriter.o
APP_OBJ += DispatchObject.o
APP_OBJ += EndpointObject.o
APP_OBJ += PointIndex.o
APP_OBJ += File.o
APP_OBJ += HttpServer.o
APP_OBJ += LimitDispatch.o
APP_OBJ += LimitRecord.o
APP_OBJ += Logger.o
APP_OBJ += LogLib.o
APP_OBJ += LuaEndpoint.o
APP_OBJ += LuaEngine.o
APP_OBJ += LuaLibraryMsg.o
APP_OBJ += LuaLibrarySys.o
APP_OBJ += LuaLibraryTime.o
APP_OBJ += LuaObject.o
APP_OBJ += MathLib.o
APP_OBJ += MetricDispatch.o
APP_OBJ += MetricRecord.o
APP_OBJ += MsgProcessor.o
APP_OBJ += MsgQ.o
APP_OBJ += PublisherDispatch.o
APP_OBJ += RecordObject.o
APP_OBJ += RecordDispatcher.o
APP_OBJ += ReportDispatch.o
APP_OBJ += SpatialIndex.o
APP_OBJ += StringLib.o
APP_OBJ += TcpSocket.o
APP_OBJ += IntervalIndex.o
APP_OBJ += TimeLib.o
APP_OBJ += TraceLib.o
APP_OBJ += Uart.o
APP_OBJ += UdpSocket.o

# Linux Platform #
APP_OBJ += Cond.o
APP_OBJ += LocalLib.o
APP_OBJ += Mutex.o
APP_OBJ += Sem.o
APP_OBJ += SockLib.o
APP_OBJ += Thread.o
APP_OBJ += Timer.o
APP_OBJ += TTYLib.o

###############################################################################
##  DEFINES

PREFIX  =   /usr/local
CONFDIR =   $(PREFIX)/etc/sliderule
OBJ_DIR =   obj
BIN_DIR =   bin
O       =   3

COPT   :=   -g -Wall -Wextra -Wreorder -O$(O) $(APPREV) $(INCLUDES)
COPT   +=   -D__x64__
COPT   +=   -D__LE__ # little endian, needs to match architecture above
COPT   +=   -D_GNU_
COPT   +=   -D_LINUX_
COPT   +=   -DNDEBUG # turns off asserts
COPT   +=   -DLUA_USE_READLINE
COPT   +=   -pthread
COPT   +=   -Wno-format-security # this poses future security risk and should be removed at some point
COPT   +=   -Wno-ignored-qualifiers # needed to suppress warnings from gtkextra header files
COPT   +=   -Wlogical-op
COPT   +=   -Wshadow
COPT   +=   -DCONFIGPATH=\"$(CONFDIR)\"
COPT   +=   -DBINID=\"sigview\"
COPT   +=   -DBUILDINFO=\"unconfigured\"
COPT   +=   `pkg-config --cflags gtkextra-3.0`
COPT   +=   -I/usr/include/lua5.3
COPT   +=   -Wno-deprecated-declarations


LOPT   :=   -lpthread
LOPT   +=   -lm
LOPT   +=   -lrt
LOPT   +=   -ldl
LOPT   +=   -llua5.3 -ldl -lreadline
LOPT   +=   -lcurl
LOPT   +=   `pkg-config --libs gtkextra-3.0`
LOPT   +=   -L/usr/lib/x86_64-linux-gnu

###############################################################################
##  TOOLS

CC      =   g++
AS      =   g++
LINKER  =   g++
AR      =   ar
NM      =   nm
OBJCPY  =   objcopy
RM      =   rm -f
CP      =   cp
DIFF    =   diff
LN      =   ln
MKDIR   =   mkdir

###############################################################################
##  COMPILER RULES

ALL_OBJ := $(addprefix $(OBJ_DIR)/, $(APP_OBJ))

$(OBJ_DIR)/%.o: %.cpp
	$(CC) -c $(COPT) -o $@ $<

$(OBJ_DIR)/%.o: %.c
	$(CC) -c $(COPT) -o $@ $<

##############################################################################
##  TARGET RULES

all: clean prep sigview

sigview: $(OBJ_DIR)/SigView.o $(ALL_OBJ)
	$(LINKER) $^ $(LOPT) -o $(BIN_DIR)/$@

prep: $(OBJ_DIR) $(BIN_DIR)

$(OBJ_DIR):
	-$(MKDIR) -p $(OBJ_DIR)

$(BIN_DIR):
	-$(MKDIR) -p $(BIN_DIR)

clean ::
	-$(RM) -R $(OBJ_DIR)
	-$(RM) -R $(BIN_DIR)
