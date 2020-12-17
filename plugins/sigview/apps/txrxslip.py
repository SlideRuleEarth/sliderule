#
# Imports
#
import sys
import logging
import json

import pandas as pd
import numpy as np

from PyQt5.QtGui import *
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *

from pyqtgraph import PlotWidget, plot
import pyqtgraph as pg

###############################################################################
# Globals
###############################################################################

COLOR_MAP = [((i * 37) % 255, (i * 141) % 255, (i * 201) % 255) for i in range(1, 1000)]

###############################################################################
# Utility
###############################################################################

class CheckableComboBox(QComboBox):

    # Subclass Delegate to increase item height
    class Delegate(QStyledItemDelegate):
        def sizeHint(self, option, index):
            size = super().sizeHint(option, index)
            size.setHeight(20)
            return size

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        # Make the combo editable to set a custom text, but readonly
        self.setEditable(True)
        self.lineEdit().setReadOnly(True)
        # Make the lineedit the same color as QPushButton
        palette = qApp.palette()
        palette.setBrush(QPalette.Base, palette.button())
        self.lineEdit().setPalette(palette)

        # Use custom delegate
        self.setItemDelegate(CheckableComboBox.Delegate())

        # Update the text when an item is toggled
        self.model().dataChanged.connect(self.updateText)

        # Hide and show popup when clicking the line edit
        self.lineEdit().installEventFilter(self)
        self.closeOnLineEditClick = False

        # Prevent popup from closing when clicking on an item
        self.view().viewport().installEventFilter(self)

    def resizeEvent(self, event):
        # Recompute text to elide as needed
        self.updateText()
        super().resizeEvent(event)

    def eventFilter(self, object, event):

        if object == self.lineEdit():
            if event.type() == QEvent.MouseButtonRelease:
                if self.closeOnLineEditClick:
                    self.hidePopup()
                else:
                    self.showPopup()
                return True
            return False

        if object == self.view().viewport():
            if event.type() == QEvent.MouseButtonRelease:
                index = self.view().indexAt(event.pos())
                item = self.model().item(index.row())

                if item.checkState() == Qt.Checked:
                    item.setCheckState(Qt.Unchecked)
                else:
                    item.setCheckState(Qt.Checked)
                return True
        return False

    def showPopup(self):
        super().showPopup()
        # When the popup is displayed, a click on the lineedit should close it
        self.closeOnLineEditClick = True

    def hidePopup(self):
        super().hidePopup()
        # Used to prevent immediate reopening when clicking on the lineEdit
        self.startTimer(100)
        # Refresh the display text when closing
        self.updateText()

    def timerEvent(self, event):
        # After timeout, kill timer, and reenable click on line edit
        self.killTimer(event.timerId())
        self.closeOnLineEditClick = False

    def updateText(self):
        texts = []
        for i in range(self.model().rowCount()):
            if self.model().item(i).checkState() == Qt.Checked:
                texts.append(self.model().item(i).text())
        text = ", ".join(texts)

        # Compute elided text (with "...")
        metrics = QFontMetrics(self.lineEdit().font())
        elidedText = metrics.elidedText(text, Qt.ElideRight, self.lineEdit().width())
        self.lineEdit().setText(elidedText)

    def addItem(self, text, data=None):
        item = QStandardItem()
        item.setText(text)
        if data is None:
            item.setData(text)
        else:
            item.setData(data)
        item.setFlags(Qt.ItemIsEnabled | Qt.ItemIsUserCheckable)
        item.setData(Qt.Unchecked, Qt.CheckStateRole)
        self.model().appendRow(item)

    def addItems(self, texts, datalist=None):
        for i, text in enumerate(texts):
            try:
                data = datalist[i]
            except (TypeError, IndexError):
                data = None
            self.addItem(text, data)

    def currentData(self):
        # Return the list of selected items data
        res = []
        for i in range(self.model().rowCount()):
            if self.model().item(i).checkState() == Qt.Checked:
                res.append(self.model().item(i).data())
        return res

#
# Danger Zone
#
def dangerzone(rws_s, rws_w, rww_s, rww_w, tx):
    lvl = 0
    gate_correction = 33
    rwe_s = rws_s + rww_s
    rwe_w = rws_w + rww_w
    
    # check t0 splitting gate starts
    rws_s_div = int((tx + rws_s - gate_correction) / 10000)
    rws_w_div = int((tx + rws_w - gate_correction) / 10000)
    if rws_s_div != rws_w_div:
        lvl += 0x01
        # check nested end
        if ((rws_s < rws_w) and (rwe_s > rwe_w)) or ((rws_w < rws_s) and (rwe_w > rwe_s)):
            lvl += 0x02
    
    return lvl

#
# Nested Exit
#
def nestedexit(transition, dz):
    return ((transition == -1.0) and (dz > 0.0)) * 1.0

#
# PyQt Plot Axis
#
class myaxis(pg.AxisItem):
    def __init__(self, *args, **kwargs):
        super(myaxis, self).__init__(*args, **kwargs)

    def tickStrings(self, values, scale, spacing):
        return [int(value*1) for value in values]

###############################################################################
# PyQt GUI
###############################################################################

#
# Application
#
class TxRxSlip(QWidget):

    # Plot Data #
    def plotData(self, start, stop):
        columnsToPlot = self.columnSelect.currentData()
        self.dataPlot.clear()
        for column,color in zip(columnsToPlot,COLOR_MAP):
            dataPen = pg.mkPen(color=color, width=3)
            self.dataPlot.plot(self.df["mfc"][start:stop], self.df[column][start:stop], name=column, pen=dataPen)
        self.dataPlot.plotItem.setAxisItems({'bottom': myaxis('bottom')})
        self.dataPlot.addLegend()

    # Display Data #
    def displayData(self, start, stop):      
        self.dataDisplay.setText(self.df.to_string(max_rows=1000, max_cols=1000))

    # Update All Data GUI Components #
    def updateData(self):
        try:
            start = int(self.startBox.text())
            stop = int(self.stopBox.text())
            self.plotData(start, stop)
            self.displayData(start, stop)
        except Exception as inst:
            print("Unable to update data: {}".format(inst))

    # Process Data #
    def processData(self):
        if "A_DFC1.HK.StrongAltRWW" in self.df.keys():
            pce = "A_DFC1.HK."
        elif "A_DFC2.HK.StrongAltRWW" in self.df.keys():
            pce = "A_DFC2.HK."
        elif "A_DFC3.HK.StrongAltRWW" in self.df.keys():
            pce = "A_DFC3.HK."

        self.df["salStart"] = (self.df[pce + "StrongAltRWS[0]"] * 65536) + (self.df[pce + "StrongAltRWS[1]"] * 256) + self.df[pce + "StrongAltRWS[2]"]
        self.df["samStart"] = (self.df[pce + "StrongAtmRWS[0]"] * 65536) + (self.df[pce + "StrongAtmRWS[1]"] * 256) + self.df[pce + "StrongAtmRWS[2]"]
        self.df["walStart"] = (self.df[pce + "WeakAltRWS[0]"] * 65536) + (self.df[pce + "WeakAltRWS[1]"] * 256) + self.df[pce + "WeakAltRWS[2]"]
        self.df["wamStart"] = (self.df[pce + "WeakAtmRWS[0]"] * 65536) + (self.df[pce + "WeakAtmRWS[1]"] * 256) + self.df[pce + "WeakAtmRWS[2]"]
        self.df["salWidth"] = self.df[pce + "StrongAltRWW"]
        self.df["samWidth"] = self.df[pce + "StrongAtmRWW"]
        self.df["walWidth"] = self.df[pce + "WeakAltRWW"]
        self.df["wamWidth"] = self.df[pce + "WeakAtmRWW"]
        self.df["mfc"] = self.df[pce + "MajorFrameCount"]
        self.df["tx"] = ((((self.df[pce + "LeadingStartTimeTag[0]"] * 65536) + (self.df[pce + "LeadingStartTimeTag[1]"] * 256) + self.df[pce + "LeadingStartTimeTag[2]"]) & 0x001FFF80) / 128) + 1
        self.df["dz"] = np.vectorize(dangerzone)(self.df['salStart'], self.df['walStart'], self.df['salWidth'], self.df['walWidth'], self.df['tx'])
        self.df["transition"] = self.df[pce + "StartDataCollection"].diff()
        self.df.at[0, "transition"] = 0.0
        self.df["nec"] = np.vectorize(nestedexit)(self.df["transition"], self.df["dz"])

        del self.df[pce + "StrongAltRWS[0]"]
        del self.df[pce + "StrongAltRWS[1]"]
        del self.df[pce + "StrongAltRWS[2]"]
        del self.df[pce + "StrongAtmRWS[0]"]
        del self.df[pce + "StrongAtmRWS[1]"]
        del self.df[pce + "StrongAtmRWS[2]"]
        del self.df[pce + "WeakAltRWS[0]"]
        del self.df[pce + "WeakAltRWS[1]"]
        del self.df[pce + "WeakAltRWS[2]"]
        del self.df[pce + "WeakAtmRWS[0]"]
        del self.df[pce + "WeakAtmRWS[1]"]
        del self.df[pce + "WeakAtmRWS[2]"]
        del self.df[pce + "LeadingStartTimeTag[0]"]
        del self.df[pce + "LeadingStartTimeTag[1]"]
        del self.df[pce + "LeadingStartTimeTag[2]"]
        del self.df[pce + "StrongAltRWW"]
        del self.df[pce + "StrongAtmRWW"]
        del self.df[pce + "WeakAltRWW"]
        del self.df[pce + "WeakAtmRWW"]

    # Open CSV File #
    def openFile(self, filename):
        try:
            self.df = pd.read_csv(filename, index_col=False)
            self.dataSize = len(self.df.index)
            self.startBox.setText("0")
            self.stopBox.setText(str(self.dataSize))
            self.processData()
            self.columnSelect.clear()
            self.columnSelect.addItems(self.df.keys())
        except Exception as inst:
            print("Unable to open file: {}".format(inst))
            self.df = None
            self.dataSize = 0
            self.startBox.setText("0")
            self.stopBox.setText("0")

    # SigView Constructor #
    def __init__(self, filename=None):
        super().__init__()

        # Plot Window
        self.dataPlot = pg.PlotWidget()
        self.dataPlot.setBackground('w')
        self.dataPlot.setLabel('bottom', "<span style=\"font-size:8px\">major frame count</span>")

        # Control Window
        controlLayout = QVBoxLayout()
        openButton = QPushButton("Open File")
        openButton.clicked.connect(self.on_open)
        plotButton = QPushButton("Plot Data")
        plotButton.clicked.connect(self.on_plot)
        startLayout = QHBoxLayout()
        startLabel = QLabel("Start MFC", self)
        self.startBox = QLineEdit(self)
        self.startBox.setText("0")
        startLayout.addWidget(startLabel)
        startLayout.addWidget(self.startBox)
        stopLayout = QHBoxLayout()
        stopLabel = QLabel("Stop MFC", self)
        self.stopBox = QLineEdit(self)
        self.stopBox.setText("0")
        stopLayout.addWidget(stopLabel)
        stopLayout.addWidget(self.stopBox)
        self.columnSelect = CheckableComboBox()
        controlLayout.addWidget(openButton)
        controlLayout.addWidget(plotButton)
        controlLayout.addLayout(startLayout)
        controlLayout.addLayout(stopLayout)
        controlLayout.addWidget(self.columnSelect)

        # Display Window
        self.dataDisplay = QTextEdit(self)
        self.dataDisplay.setFont(QFont('Consolas', 9)) 
        self.dataDisplay.setReadOnly(True)
        self.dataDisplay.setLineWrapMode(QTextEdit.NoWrap)

        # Window Layout
        bottomLayout = QHBoxLayout()
        bottomLayout.addLayout(controlLayout, 1)
        bottomLayout.addWidget(self.dataDisplay, 7)        
        mainLayout = QVBoxLayout()
        mainLayout.addWidget(self.dataPlot, 3)
        mainLayout.addLayout(bottomLayout, 2)
        self.setLayout(mainLayout)

        # Load Default DataFrame
        self.dataSize = 0
        self.df = None
        if filename is not None:
            self.openFile(filename)

        # Display Data
        self.updateData()

        # Window
        self.title = 'TxRxSlip'
        self.setWindowTitle(self.title)
        self.show()

    @pyqtSlot()
    def on_open(self):
        filename,ok = QFileDialog.getOpenFileName(None, "Select a file...", '.', filter="All files (*)")
        if ok:
            self.openFile(filename)

    @pyqtSlot()
    def on_plot(self):
        self.updateData()

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Read Default Filename #
    filename = None
    if len(sys.argv) > 1:
        filename = sys.argv[1]

    # Start GUI
    app = QApplication(sys.argv)
    ex = TxRxSlip(filename)
    sys.exit(app.exec_())
