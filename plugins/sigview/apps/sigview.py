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
class App(QWidget):

    def __init__(self):
        super().__init__()

        # Plot Window #
        self.histPlot = pg.PlotWidget()
        hour = [1,2,3,4,5,6,7,8,9,10]
        temperature = [30,32,34,32,33,31,29,32,35,45]
        self.histPlot.plot(hour, temperature)

        # Slider #
        self.histSlider = QSlider()
        self.histSlider.setMinimum(0)
        self.histSlider.setMaximum(100)
        self.histSlider.setTickPosition(QSlider.TicksLeft)
        self.histSlider.setOrientation(Qt.Horizontal)

        # Filter
        self.select_all_box = QCheckBox("Select All")
        self.select_all_box.setChecked(True)
        self.select_all_box.clicked.connect(self.on_click_select_all)

        # Attributes
        self.histAttr = QTextEdit(self)
        self.histAttr.setFont(QFont('Consolas', 9)) 
        self.histAttr.setReadOnly(True)
        opening_message = "Histogram Attributes\n\n"
        self.histAttr.setText(opening_message)
#        self.histAttr.setFixedWidth(self.histAttr.sizeHint().width() + 650)
#        self.histAttr.setFixedHeight(self.histAttr.sizeHint().height() + 400)

        # Bottom Pane Layout
        bottomLayout = QHBoxLayout()
        bottomLayout.addWidget(self.select_all_box, 1)
        bottomLayout.addWidget(self.histAttr, 1)

        # Window Layout
        mainLayout = QVBoxLayout()
        mainLayout.addWidget(self.histPlot, 10)
        mainLayout.addWidget(self.histSlider, 1)
        mainLayout.addLayout(bottomLayout, 1)
        self.setLayout(mainLayout)

        # Window
        self.title = 'SigView'
        self.setWindowTitle(self.title)
        self.show()

    @pyqtSlot()
    def on_click_open(self):
        options = QFileDialog.Options()
        csv_file, _ = QFileDialog.getOpenFileName(self,"QFileDialog.getOpenFileName()", "","All Files (*);;CSV Files (*.csv)", options=options)


    @pyqtSlot()
    def on_click_select_all(self):
        print("Select All")
        
def main_app():
    app = QApplication(sys.argv)
    ex = App()
    sys.exit(app.exec_())

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
#    rsps = atl00exec(parms)
    
    # Start GUI
    main_app()
