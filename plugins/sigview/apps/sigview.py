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

    def processHists(self, rsps):
        altHist = []
        if "AltHist" in rsps:
            altHist = rsps["AltHist"]
        tagHist = []
        if "TagHist" in rsps:
            tagHist = rsps["TagHist"]
        self.histSet = altHist + tagHist
        self.numHists = len(self.histSet)
        self.histCounts = {}
        self.histTotalCount = 0
        for histType in self.histTypes:
            self.histCounts[histType] = {}
            for pceNum in self.pceNums:
                self.histCounts[histType][pceNum] = 0
        for hist in self.histSet:
            if hist["TYPE"] in self.histMapping and hist["PCE"] >= 0 and hist["PCE"] <= 2:
                histType = self.histMapping[hist["TYPE"]]
                pceNum = hist["PCE"] + 1
                self.histCounts[histType][pceNum] += 1
                self.histTotalCount += 1

    def displayHistAttributes(self, index):
        displayText = ""
        try:
            hist = self.histSet[index]
            displayText = """
            GPS: {}
            PCE:        {:8}     
            MFC:        {:8}
            TYPE:       {:8}
            INTPERIOD:  {:8}
            BINSIZE:    {:8.3f}
            SUM:        {}
            """.format(hist["GPSSTR"], hist["PCE"]+1, hist["MFC"], self.histMapping[hist["TYPE"]], hist["INTPERIOD"], hist["BINSIZE"], hist["SUM"])
        except Exception as inst:
            displayText = "Error displaying attributes for histogram {}: {}".format(index, inst)
        self.histAttr.setText(displayText)

    def plotHistBins(self, index):
        try:
            hist = self.histSet[index]
            bins = hist["BINS"]
            x = [i * hist["BINSIZE"] for i in range(len(bins))]
            self.histPlot.clear()
            self.histPlot.plot(x, bins)
        except Exception as inst:
            print("Error plotting histogram {}: {}".format(index, inst))

    def updateHist(self, index):
        self.sliderLabel.setText("{}".format(index))
        self.plotHistBins(index)
        self.displayHistAttributes(index)

    def __init__(self, rsps):
        super().__init__()

        # Histogram Data
        self.histMapping = {0: "SAL", 1: "WAL", 2: "SAM", 3: "WAM", 4: "STT", 5: "WTT"}
        self.histTypes = ["STT", "WTT", "SAL", "WAL", "SAM", "WAM"]
        self.pceNums = [1, 2, 3]
        self.processHists(rsps)

        # Plot Window #
        self.histPlot = pg.PlotWidget()
        self.plotHistBins(0)

        # Slider #
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
        self.filters = {}
        for histType in self.histTypes:
            self.filters[histType] = {}
        filterLayout = QVBoxLayout()
        selectAllLayout = QHBoxLayout()
        self.selectAllBox = QCheckBox("")
        self.selectAllBox.setChecked(True)
        self.selectAllBox.clicked.connect(self.on_click_select_all)
        selectAllLayout.addWidget(QLabel("Select All", self), 1)
        selectAllLayout.addWidget(self.selectAllBox, 1)
        selectAllLayout.addWidget(QLabel("{}".format(self.histTotalCount), self), 10)
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
                self.filters[histType][pceNum] = QCheckBox("{}".format(self.histCounts[histType][pceNum]))
                self.filters[histType][pceNum].setChecked(True)
                typeLayout.addWidget(self.filters[histType][pceNum], 1)
            filterLayout.addLayout(typeLayout)
                
        # Attributes
        self.histAttr = QTextEdit(self)
        self.histAttr.setFont(QFont('Consolas', 9)) 
        self.histAttr.setReadOnly(True)
        self.displayHistAttributes(0)
        self.histAttr.setFixedWidth(self.histAttr.sizeHint().width() + 100)
        self.histAttr.setFixedHeight(self.histAttr.sizeHint().height() + 40)

        # Bottom Pane Layout
        bottomLayout = QHBoxLayout()
        bottomLayout.addLayout(filterLayout, 1)
        bottomLayout.addWidget(self.histAttr, 1)

        # Window Layout
        mainLayout = QVBoxLayout()
        mainLayout.addWidget(self.histPlot, 10)
        mainLayout.addLayout(sliderLayout, 1)
        mainLayout.addLayout(bottomLayout, 1)
        self.setLayout(mainLayout)

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
            self.histSlider.setValue(index - 1)
        self.updateHist(index)

    @pyqtSlot()
    def on_slider_right(self):
        index = self.histSlider.value()
        if index < self.numHists - 1:
            self.histSlider.setValue(index + 1)
        self.updateHist(index)

    @pyqtSlot()
    def on_click_select_all(self):
        for f in self.filters:
            for p in [1,2,3]:
                self.filters[f][p].setChecked(self.selectAllBox.isChecked())
        
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
    rsps = atl00exec(parms)
    
    # Start GUI
    app = QApplication(sys.argv)
    ex = SigView(rsps)
    sys.exit(app.exec_())
