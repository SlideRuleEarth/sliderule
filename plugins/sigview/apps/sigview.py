#
# Imports
#
import sys
import logging
import json

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
    rsps  = sliderule.engine("atl00", parms)

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
        tagText = "tag"
        channelText = "channel"
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
""".format( hist["GPSSTR"],
            hist["PCE"]+1,  hist["TXCNT"],
            hist["MFC"],  hist["SUM"],
            self.histMapping[hist["TYPE"]],  hist["SIZE"],
            hist["INTPERIOD"],  hist["RWDROPOUT"],
            hist["BINSIZE"] * (10. / 1.5),  hist["DIDNOTFINISHTX"],
            hist["RWS"], hist["DIDNOTFINISHWR"],
            hist["RWW"], hist["DFCEDAC"],
            hist["BKGND"], hist["SDRAMMISMATCH"],
            hist["SIGRNG"], hist["TRACKINGFIFO"],
            hist["SIGWID"], hist["STARTTAGFIFO"],
            hist["SIGPES"], hist["DFCEDAC"])
            # Tag Text
            if
            tagText = """GPS: {}
PCE:        {:11}
MFC:        {:11}
DLB_START:  {:11} {:5} {:5}
DLB_MASK:   {:11x} {:5x} {:5x}
DLB_TAGCNT: {:11} {:5} {:5}
RWS:        {:11.0f} 
RWW:        {:11.0f} 
BKGND:      {:11.3f}
SIGRNG:     {:11.3f}
SIGWID:     {:11.1f}
SIGPES:     {:11.3f}
""".format( hist["GPSSTR"],
            hist["PCE"]+1,  hist["TXCNT"],
            hist["MFC"],  hist["SUM"],
            self.histMapping[hist["TYPE"]],  hist["SIZE"],
            hist["INTPERIOD"],  hist["RWDROPOUT"],
            hist["BINSIZE"] * (10. / 1.5),  hist["DIDNOTFINISHTX"],
            hist["RWS"], hist["DIDNOTFINISHWR"],
            hist["RWW"], hist["DFCEDAC"],
            hist["BKGND"], hist["SDRAMMISMATCH"],
            hist["SIGRNG"], hist["TRACKINGFIFO"],
            hist["SIGWID"], hist["STARTTAGFIFO"],
            hist["SIGPES"], hist["DFCEDAC"])
            # Channel Text

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
            x = [i * hist["BINSIZE"] * (10.0 / 1.5) for i in range(len(bins))]
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
        self.histFullSet = self.altHist + self.tagHist  
        self.histWorkingSet = self.histFullSet 
        self.numHists = len(self.histWorkingSet)

        # Plot Window
        self.histPlot = pg.PlotWidget()
        self.histPlot.showGrid(x=True, y=True)
        self.histPlot.setLabel('left', "<span style=\"font-size:8px\">photons (counts)</span>")
        self.histPlot.setLabel('bottom', "<span style=\"font-size:8px\">range window (ns)</span>")
        self.histPen = pg.mkPen(color=(173,216,230))

        # Slider
        sliderLayout = QHBoxLayout()
        self.histSlider = QSlider()
        self.histSlider.setRange(0, self.numHists - 1)
        self.histSlider.setPageStep(int(self.numHists / 20))
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
                
        # Attributes
        self.histTabs = QTabWidget()
        self.basicAttr = self.createAttrDisplay()
        self.tagAttr = self.createAttrDisplay()
        self.channelAttr = self.createAttrDisplay()
        self.histTabs.addTab(self.basicAttr,"Basic")
        self.histTabs.addTab(self.tagAttr,"Tag")
        self.histTabs.addTab(self.channelAttr,"Channel")

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
            self.updateHist(index)

    @pyqtSlot()
    def on_slider_right(self):
        index = self.histSlider.value()
        if index < self.numHists - 1:
            index += 1
            self.histSlider.setValue(index)
            self.updateHist(index)

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
        self.histSlider.setPageStep(int(self.numHists / 20))
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
        self.histSlider.setPageStep(int(self.numHists / 20))
        self.histSlider.setValue(new_index)
        self.updateHist(new_index)

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
    url = "http://127.0.0.1:9081"
    if len(sys.argv) > 2:
        url = sys.argv[2]

    # Initialize Icesat2 Package #
    icesat2.init(url, True)

    # Execute SlideRule Algorithm
    # rsps = atl00exec(parms)
    rsps = [] 

    # Start GUI
    app = QApplication(sys.argv)
    ex = SigView(rsps)
    sys.exit(app.exec_())
