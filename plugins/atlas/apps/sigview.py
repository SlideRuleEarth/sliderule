#
# Imports
#
import sys
import logging
import json
import copy

from sliderule import sliderule, icesat2

from PyQt5.QtGui import *
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *

from pyqtgraph import PlotWidget, plot
import pyqtgraph as pg

###############################################################################
# SlideRule
###############################################################################

#
# SlideRule Processing Request
#
def atl00exec(parms):

    # Request ATL00 Processed Data
    rsps  = sliderule.source("atl00", parms, stream=True)

    # Flatten Response
    flattened = {}
    for rsp in rsps:
        rectype = rsp["@rectype"]
        if rectype not in flattened:
            flattened[rectype] = [rsp]
        else:
            flattened[rectype].append(rsp)

    return flattened


###############################################################################
# PyQt GUI
###############################################################################

#
# Application
#
class SigView(QWidget):

    # Generate Filters #
    def generateFilters(self, ):
        self.filters = {}
        self.filterCounts = {}
        self.filterTotalCount = 0
        self.filterCounts = {}
        self.filterTotalCount = 0
        for histType in self.histTypes:
            self.filters[histType] = {}
            self.filterCounts[histType] = {}
            for pceNum in self.pceNums:
                self.filterCounts[histType][pceNum] = 0
        for hist in self.histFullSet:
            if hist["TYPE"] in self.histMapping and hist["PCE"] >= 0 and hist["PCE"] <= 2:
                histType = self.histMapping[hist["TYPE"]]
                pceNum = hist["PCE"] + 1
                self.filterCounts[histType][pceNum] += 1
                self.filterTotalCount += 1

    # Create Attribute Display #
    def createAttrDisplay(self):
        attr_display = QTextEdit(self)
        attr_display.setFont(QFont('Consolas', 9)) 
        attr_display.setReadOnly(True)
        attr_display.setLineWrapColumnOrWidth(600)
        attr_display.setLineWrapMode(QTextEdit.FixedPixelWidth)
        attr_display.setFixedHeight(attr_display.sizeHint().height() + 20)
        return attr_display

    # Display Histogram Attributes #
    def displayHistAttributes(self, index):
        basicText = ""
        tagText = "No tag information available"
        channelText = "No channel information available"
        try:
            hist = self.histWorkingSet[index]
            # Basic Text
            basicText = """GPS: {}
PCE:        {:11}          TXCNT:      {:8} shots    
MFC:        {:11}          SUM:        {:8} photons
TYPE:       {:>11}          SIZE:       {:8} bins
INTPERIOD:  {:11} mf       RWDROPOUT:  {:8}
BINSIZE:    {:11.3f} ns       DIDNOTFINTX:{:8}
RWS:        {:11.0f} ns       DIDNOTFINWR:{:8}
RWW:        {:11.0f} ns       DFCEDAC:    {:8}
BKGND:      {:11.3f} MHz      SDRAMMIS:   {:8}
SIGRNG:     {:11.3f} ns       TRACKFIFO:  {:8}
SIGWID:     {:11.1f} ns       STARTFIFO:  {:8}
SIGPES:     {:11.3f} ns       DFCSTATUS:  {:8x}
BKGNDCNTS:  {:11} {:5}    MFP:        {:8}
""".format( hist["GPSSTR"],
            hist["PCE"]+1,  hist["TXCNT"],
            hist["MFC"],  hist["SUM"],
            self.histMapping[hist["TYPE"]],  hist["SIZE"],
            hist["INTPERIOD"],  hist["RWDROPOUT"],
            hist["BINSIZE"],  hist["DIDNOTFINISHTX"],
            hist["RWS"], hist["DIDNOTFINISHWR"],
            hist["RWW"], hist["DFCEDAC"],
            hist["BKGND"], hist["SDRAMMISMATCH"],
            hist["SIGRNG"], hist["TRACKINGFIFO"],
            hist["SIGWID"], hist["STARTTAGFIFO"],
            hist["SIGPES"], hist["DFCSTATUS"],
            sum(hist["BKGNDCNTS"][:4]), sum(hist["BKGNDCNTS"][4:]), hist["MFP"])
            # Tag Text
            if hist["TYPE"] == 4 or hist["TYPE"] == 5:
                tagText = """GPS: {}
PCE:        {:11}
MFC:        {:11}
DLB_START:  {:8} {:8} {:8} {:8}
DLB_WIDTH:  {:8} {:8} {:8} {:8}
DLB_MASK:   {:8x} {:8x} {:8x} {:8x}
DLB_TAGCNT: {:8} {:8} {:8} {:8}

MFC HDR FMT DLB TAG PKT     TAGS MIN  MAX  AVG
{:3} {:3} {:3} {:3} {:3} {:3}          {:3}  {:3}  {:3.1f}
""".format( hist["GPSSTR"],
            hist["PCE"]+1,
            hist["MFC"],
            hist["DLB0_START"], hist["DLB1_START"], hist["DLB2_START"], hist["DLB3_START"],
            hist["DLB0_WIDTH"], hist["DLB1_WIDTH"], hist["DLB2_WIDTH"], hist["DLB3_WIDTH"],
            hist["DLB0_MASK"], hist["DLB1_MASK"], hist["DLB2_MASK"], hist["DLB3_MASK"],
            hist["DLB0_TAGCNT"], hist["DLB1_TAGCNT"], hist["DLB2_TAGCNT"], hist["DLB3_TAGCNT"],
            hist["MFC_ERRORS"], hist["HDR_ERRORS"], hist["FMT_ERRORS"], hist["DLB_ERRORS"], hist["TAG_ERRORS"], hist["PKT_ERRORS"],
            hist["MIN_TAGS"], hist["MAX_TAGS"], hist["AVG_TAGS"])
            # Channel Text
            if hist["TYPE"] == 5:
                channelText = """GPS: {}
PCE:        {:11}
MFC:        {:11}
                17     18     19     20
CHCNT:      {:6} {:6} {:6} {:6}
CHBIAS:     {:6.3f} {:6.3f} {:6.3f} {:6.3f}
""".format( hist["GPSSTR"],
            hist["PCE"]+1,
            hist["MFC"],
            hist["CHCNT"][16], hist["CHCNT"][17], hist["CHCNT"][18], hist["CHCNT"][19],
            hist["CHBIAS"][16], hist["CHBIAS"][17], hist["CHBIAS"][18], hist["CHBIAS"][19])
            if hist["TYPE"] == 4:
                channelText = """GPS: {}
PCE:        {:11}
MFC:        {:11}
                 1      2      3      4      5      6      7     8
CHCNT:      {:6} {:6} {:6} {:6} {:6} {:6} {:6} {:6}
CHBIAS:     {:6.3f} {:6.3f} {:6.3f} {:6.3f} {:6.3f} {:6.3f} {:6.3f} {:6.3f}

                 9     10     11     12     13     14     15     16
CHCNT:      {:6} {:6} {:6} {:6} {:6} {:6} {:6} {:6}
CHBIAS:     {:6.3f} {:6.3f} {:6.3f} {:6.3f} {:6.3f} {:6.3f} {:6.3f} {:6.3f}
""".format( hist["GPSSTR"],
            hist["PCE"]+1,
            hist["MFC"],
            hist["CHCNT"][0], hist["CHCNT"][1], hist["CHCNT"][2], hist["CHCNT"][3], hist["CHCNT"][4], hist["CHCNT"][5], hist["CHCNT"][6], hist["CHCNT"][7], 
            hist["CHBIAS"][0], hist["CHBIAS"][1], hist["CHBIAS"][2], hist["CHBIAS"][3], hist["CHBIAS"][4], hist["CHBIAS"][5], hist["CHBIAS"][6], hist["CHBIAS"][7], 
            hist["CHCNT"][8], hist["CHCNT"][9], hist["CHCNT"][10], hist["CHCNT"][11], hist["CHCNT"][12], hist["CHCNT"][13], hist["CHCNT"][14], hist["CHCNT"][15],
            hist["CHBIAS"][8], hist["CHBIAS"][9], hist["CHBIAS"][10], hist["CHBIAS"][11], hist["CHBIAS"][12], hist["CHBIAS"][13], hist["CHBIAS"][14], hist["CHBIAS"][15])
        except Exception as inst:
            basicText = "Nothing to display"
            tagText = "Nothing to display"
            channelText = "Nothing to display"
            print("Unable to display attributes for histogram {}: {}".format(index, inst))
        self.basicAttr.setText(basicText)
        self.tagAttr.setText(tagText)
        self.channelAttr.setText(channelText)

    # Plot Histogram Bins #
    def plotHistBins(self, index):
        try:
            hist = self.histWorkingSet[index]
            bins = hist["BINS"]
            x = [i * hist["BINSIZE"] for i in range(len(bins))]
            self.histPlot.clear()
            self.histPlot.plot(x, bins, pen=self.histPen)
        except Exception as inst:
            print("Unable to plot histogram {}: {}".format(index, inst))

    # Update All Histogram Displays #
    def updateHist(self, index):
        self.sliderLabel.setText("{} / {}".format(index,self.numHists))
        self.plotHistBins(index)
        self.displayHistAttributes(index)

    # SigView Constructor #
    def __init__(self, rsps):
        super().__init__()

        # Histogram Data
        self.histMapping = {0: "SAL", 1: "WAL", 2: "SAM", 3: "WAM", 4: "STT", 5: "WTT"}
        self.histTypes = ["STT", "WTT", "SAL", "WAL", "SAM", "WAM"]
        self.pceNums = [1, 2, 3]
        self.altHist = rsps["AltHist"] if "AltHist" in rsps else []
        self.tagHist = rsps["TagHist"] if "TagHist" in rsps else []
        self.histOriginalSet = self.altHist + self.tagHist 
        self.histFullSet = self.histOriginalSet
        self.histWorkingSet = self.histFullSet 
        self.numHists = len(self.histWorkingSet)
        self.histIntegration = 1

        # Plot Window
        self.histPlot = pg.PlotWidget()
        self.histPlot.showGrid(x=True, y=True)
        self.histPlot.setLabel('left', "<span style=\"font-size:8px\">photons (counts)</span>")
        self.histPlot.setLabel('bottom', "<span style=\"font-size:8px\">range window (ns)</span>")
        self.histPen = pg.mkPen(color=(173,216,230))

        # Slider
        sliderLayout = QHBoxLayout()
        self.histSliderPageSize = 1000
        self.histSlider = QSlider()
        self.histSlider.setRange(0, self.numHists - 1)
        self.histSlider.setPageStep(int(self.numHists / self.histSliderPageSize))
        self.histSlider.setTickPosition(QSlider.TicksLeft)
        self.histSlider.setOrientation(Qt.Horizontal)
        self.histSlider.valueChanged.connect(self.on_slider_changed)
        sliderLeft = QPushButton()
        sliderLeft.setIcon(self.style().standardIcon(getattr(QStyle, 'SP_ArrowLeft')))
        sliderLeft.clicked.connect(self.on_slider_left)
        self.sliderLabel = QLabel("0", self)
        sliderRight = QPushButton()
        sliderRight.setIcon(self.style().standardIcon(getattr(QStyle, 'SP_ArrowRight')))
        sliderRight.clicked.connect(self.on_slider_right)
        sliderLayout.addWidget(self.histSlider)
        sliderLayout.addWidget(sliderLeft)
        sliderLayout.addWidget(self.sliderLabel)
        sliderLayout.addWidget(sliderRight)

        # Filter
        self.generateFilters()        
        filterLayout = QVBoxLayout()
        selectAllLayout = QHBoxLayout()
        self.selectAllBox = QCheckBox("")
        self.selectAllBox.setChecked(True)
        self.selectAllBox.clicked.connect(self.on_click_select_all)
        selectAllLayout.addWidget(QLabel("Select All", self), 1)
        selectAllLayout.addWidget(self.selectAllBox, 1)
        selectAllLayout.addWidget(QLabel("{}".format(self.filterTotalCount), self), 10)
        filterLayout.addLayout(selectAllLayout)
        pceLabelLayout = QHBoxLayout()
        pceLabelLayout.addWidget(QLabel("PCE", self), 1)
        pceLabelLayout.addWidget(QLabel("1", self), 1)
        pceLabelLayout.addWidget(QLabel("2", self), 1)
        pceLabelLayout.addWidget(QLabel("3", self), 1)
        filterLayout.addLayout(pceLabelLayout)
        for histType in self.filters:
            typeLayout = QHBoxLayout()
            typeLayout.addWidget(QLabel("{}".format(histType), self), 1)
            for pceNum in self.pceNums:
                self.filters[histType][pceNum] = QCheckBox("{}".format(self.filterCounts[histType][pceNum]))
                self.filters[histType][pceNum].setChecked(True)
                self.filters[histType][pceNum].clicked.connect(self.on_click_filter)
                typeLayout.addWidget(self.filters[histType][pceNum], 1)
            filterLayout.addLayout(typeLayout)
        
        # Analysis
        analysisGbox = QGroupBox()
        analysisVbox = QVBoxLayout()
        self.progress = QProgressBar(self)
        analysisVbox.addWidget(self.progress)
        peakAlignRow = QHBoxLayout()
        peakAlignButton = QPushButton("Peak Align")
        self.integrateLabel = QLabel("{}".format(self.histIntegration))
        peakAlignButton.clicked.connect(self.on_click_peak_align)
        restoreButton = QPushButton("Restore")
        restoreButton.clicked.connect(self.on_click_restore)
        peakAlignRow.addWidget(peakAlignButton)
        peakAlignRow.addWidget(self.integrateLabel)
        analysisVbox.addLayout(peakAlignRow)
        analysisVbox.addWidget(restoreButton)
        analysisGbox.setLayout(analysisVbox)

        # Tabs
        self.histTabs = QTabWidget()
        self.basicAttr = self.createAttrDisplay()
        self.tagAttr = self.createAttrDisplay()
        self.channelAttr = self.createAttrDisplay()
        self.histTabs.addTab(self.basicAttr,"Basic")
        self.histTabs.addTab(self.tagAttr,"Tag")
        self.histTabs.addTab(self.channelAttr,"Channel")
        self.histTabs.addTab(analysisGbox,"Analysis")

        # Bottom Pane Layout
        bottomLayout = QHBoxLayout()
        bottomLayout.addLayout(filterLayout, 1)
        bottomLayout.addWidget(self.histTabs, 3)

        # Window Layout
        mainLayout = QVBoxLayout()
        mainLayout.addWidget(self.histPlot, 7)
        mainLayout.addLayout(sliderLayout, 1)
        mainLayout.addLayout(bottomLayout, 1)
        self.setLayout(mainLayout)

        # Display Initial Histogram
        self.updateHist(0)

        # Window
        self.title = 'SigView'
        self.setWindowTitle(self.title)
        self.show()

    @pyqtSlot()
    def on_slider_changed(self):
        index = self.histSlider.value()
        self.updateHist(index)

    @pyqtSlot()
    def on_slider_left(self):
        index = self.histSlider.value()
        if index > 0:
            index -= 1
            self.histSlider.setValue(index)

    @pyqtSlot()
    def on_slider_right(self):
        index = self.histSlider.value()
        if index < self.numHists - 1:
            index += 1
            self.histSlider.setValue(index)

    @pyqtSlot()
    def on_click_select_all(self):
        select_all = self.selectAllBox.isChecked()
        new_index = self.histSlider.value()
        # set filters
        for f in self.filters:
            for p in [1,2,3]:
                self.filters[f][p].setChecked(select_all)
        # set working histograms
        if select_all:
            self.histWorkingSet = self.histFullSet        
        else:
            self.histWorkingSet = []
            new_index = 0
        self.numHists = len(self.histWorkingSet)
        self.histSlider.setRange(0, self.numHists - 1)
        self.histSlider.setPageStep(int(self.numHists / self.histSliderPageSize))
        self.histSlider.setValue(new_index)
        self.updateHist(new_index)

    @pyqtSlot()
    def on_click_filter(self):
        index = self.histSlider.value()
        curr_hist = self.histWorkingSet[index]
        new_index = 0
        curr_index = 0
        self.histWorkingSet = []     
        for hist in self.histFullSet:
            if hist["TYPE"] in self.histMapping and hist["PCE"] >= 0 and hist["PCE"] <= 2:
                histType = self.histMapping[hist["TYPE"]]
                pceNum = hist["PCE"] + 1
                if curr_hist == hist:
                    new_index = curr_index
                if self.filters[histType][pceNum].isChecked():
                    self.histWorkingSet.append(hist)
                    curr_index += 1
        self.numHists = len(self.histWorkingSet)
        self.histSlider.setRange(0, self.numHists - 1)
        self.histSlider.setPageStep(int(self.numHists / self.histSliderPageSize))
        self.histSlider.setValue(new_index)
        self.updateHist(new_index)

    @pyqtSlot()
    def on_click_peak_align(self):
        integration, okPressed = QInputDialog.getInt(self, "Histgram Integration Period","Major Frames:", self.histIntegration, 0, 500, 1)
        if okPressed:
            self.histIntegration = integration
            self.integrateLabel.setText("{}".format(self.histIntegration))

            # Build Empty Integrations #
            currHists = {}
            currCounts = {}
            for histType in self.histTypes:
                currHists[histType] = {}
                currCounts[histType] = {}
                for pceNum in self.pceNums:
                    currHists[histType][pceNum] = None
                    currCounts[histType][pceNum] = 0

            # Set Progress Bar
            self.progress.setMaximum(len(self.histOriginalSet))

            # Integrate Histogram
            progressCnt = 0
            totalBins = 2000
            centerBin = 1000
            self.histWorkingSet = []     
            for hist in self.histOriginalSet:
                if hist["TYPE"] in self.histMapping and hist["PCE"] >= 0 and hist["PCE"] <= 2:
                    histType = self.histMapping[hist["TYPE"]]
                    pceNum = hist["PCE"] + 1
                    if currCounts[histType][pceNum] == 0:
                        currHists[histType][pceNum] = copy.deepcopy(hist)
                        currCounts[histType][pceNum] = 1
                        currHists[histType][pceNum]["BINS"] = [0 for i in range(totalBins)]
                        currHists[histType][pceNum]["CHCNT"] = [cnt for cnt in currHists[histType][pceNum]["CHCNT"]]
                        currHists[histType][pceNum]["CHBIAS"] = [cnt for cnt in currHists[histType][pceNum]["CHBIAS"]]
                    else:
                        currHist = currHists[histType][pceNum]
                        currCounts[histType][pceNum] += 1
                        # sums
                        currHist["TXCNT"]           += hist["TXCNT"]
                        currHist["SUM"]             += hist["SUM"]
                        currHist["INTPERIOD"]       += hist["INTPERIOD"]
                        currHist["RWDROPOUT"]       += hist["RWDROPOUT"]
                        currHist["DIDNOTFINISHTX"]  += hist["DIDNOTFINISHTX"]
                        currHist["DIDNOTFINISHWR"]  += hist["DIDNOTFINISHWR"]
                        currHist["DFCEDAC"]         += hist["DFCEDAC"]
                        currHist["SDRAMMISMATCH"]   += hist["SDRAMMISMATCH"]
                        currHist["TRACKINGFIFO"]    += hist["TRACKINGFIFO"]
                        currHist["STARTTAGFIFO"]    += hist["STARTTAGFIFO"]
                        currHist["DFCEDAC"]         += hist["DFCEDAC"]
                        # averages
                        currHist["BKGND"]           += hist["BKGND"]
                        currHist["SIGRNG"]          += hist["SIGRNG"]
                        currHist["SIGWID"]          += hist["SIGWID"]
                        currHist["SIGPES"]          += hist["SIGPES"]
                        # time tag histograms only #
                        if hist["TYPE"] == 4 or hist["TYPE"] == 5:
                            # sums
                            currHist["DLB0_TAGCNT"] += hist["DLB0_TAGCNT"]
                            currHist["DLB1_TAGCNT"] += hist["DLB1_TAGCNT"]
                            currHist["DLB2_TAGCNT"] += hist["DLB2_TAGCNT"] 
                            currHist["DLB3_TAGCNT"] += hist["DLB3_TAGCNT"]
                            currHist["MFC_ERRORS"]  += hist["MFC_ERRORS"] 
                            currHist["HDR_ERRORS"]  += hist["HDR_ERRORS"] 
                            currHist["FMT_ERRORS"]  += hist["FMT_ERRORS"] 
                            currHist["DLB_ERRORS"]  += hist["DLB_ERRORS"] 
                            currHist["TAG_ERRORS"]  += hist["TAG_ERRORS"]
                            currHist["PKT_ERRORS"]  += hist["PKT_ERRORS"]
                            for c in range(20):
                                currHist["CHCNT"][c]+= hist["CHCNT"][c]                                
                            # min and max
                            if currHist["MIN_TAGS"] > hist["MIN_TAGS"]:
                                currHist["MIN_TAGS"] = hist["MIN_TAGS"]
                            if currHist["MAX_TAGS"] > hist["MAX_TAGS"]:
                                currHist["MAX_TAGS"] = hist["MAX_TAGS"]
                            # averages
                            currHist["AVG_TAGS"]    += hist["AVG_TAGS"]
                            for c in range(20):
                                currHist["CHBIAS"][c] += hist["CHBIAS"][c]
                    # rebin histogram
                    newBins = currHists[histType][pceNum]["BINS"]
                    oldBins = hist["BINS"]
                    signalBin = (hist["SIGRNG"] - hist["RWS"]) / hist["BINSIZE"]
                    binOffset = int((centerBin - signalBin) + 0.5)
                    for b in range(len(hist["BINS"])):
                        newBin = b + binOffset
                        if newBin >= 0 and newBin < totalBins:
                            newBins[newBin] += oldBins[b]
                    # integration period reached
                    if currCounts[histType][pceNum] == self.histIntegration:
                        currHists[histType][pceNum]["BKGND"]  /= self.histIntegration
                        currHists[histType][pceNum]["SIGRNG"] /= self.histIntegration
                        currHists[histType][pceNum]["SIGWID"] /= self.histIntegration
                        currHists[histType][pceNum]["SIGPES"] /= self.histIntegration
                        if currHists[histType][pceNum]["TYPE"] == 4 or currHists[histType][pceNum]["TYPE"] == 5:
                            currHists[histType][pceNum]["AVG_TAGS"] /= self.histIntegration
                            for c in range(20):
                                currHists[histType][pceNum]["CHBIAS"][c] /= self.histIntegration  
                        # append and reset
                        self.histWorkingSet.append(currHists[histType][pceNum])
                        currHists[histType][pceNum] = None
                        currCounts[histType][pceNum] = 0
                # update progress
                progressCnt += 1
                self.progress.setValue(progressCnt)
            # update histogram sets
            self.histFullSet = self.histWorkingSet
            self.numHists = len(self.histWorkingSet)
            # reset display
            self.histSlider.setRange(0, self.numHists - 1)
            self.histSlider.setPageStep(int(self.numHists / self.histSliderPageSize))
            self.histSlider.setValue(0)
            self.progress.setValue(0)
            self.updateHist(0)

    @pyqtSlot()
    def on_click_restore(self):
        self.histFullSet = self.histOriginalSet
        self.histWorkingSet = self.histFullSet
        self.numHists = len(self.histWorkingSet)
        self.histSlider.setRange(0, self.numHists - 1)
        self.histSlider.setPageStep(int(self.numHists / self.histSliderPageSize))
        self.histSlider.setValue(0)
        self.progress.setValue(0)
        self.updateHist(0)



###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # configure logging
    logging.basicConfig(level=logging.INFO)

    # Read Request #
    parms_filename = sys.argv[1]
    with open(parms_filename) as parmfile:
        parms = json.load(parmfile)

    # Set URL #
    url = "http://127.0.0.1:9082"
    if len(sys.argv) > 2:
        url = sys.argv[2]

    # Initialize Icesat2 Package #
    icesat2.init(url, True)

    # Execute SlideRule Algorithm
    rsps = atl00exec(parms)

    # Start GUI
    if parms["silent"] == False:
        app = QApplication(sys.argv)
        ex = SigView(rsps)
        sys.exit(app.exec_())
