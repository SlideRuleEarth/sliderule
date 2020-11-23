/*-----------------------------------------------------------------------------
--
-- Module Name: Charter.cpp
--
-- Description:
--
--   Encapsulation of GTK Charter Application
--
-- Notes:
--
-----------------------------------------------------------------------------*/

/******************************************************************************
 INCLUDES
 ******************************************************************************/

#include <gtk/gtk.h>
#include <gtkextra/gtkextra.h>
#include <pthread.h>
#include <stdio.h>
#include "errno.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <math.h>
#include <semaphore.h>

#include "rtap/core.h"
#include "rtap/ccsds.h"

#include "Charter.h"

/******************************************************************************
 STATIC DATA
 ******************************************************************************/

const char* Charter::protocol_list[] = {"ASCII", "BINARY", "SIS", "ADAS", "NTGSE", "ADASFILE", "ITOSARCH", "DATASRV"};
#define NUM_PROTOCOLS ((int)(sizeof(Charter::protocol_list) / sizeof(const char*)))

const char* Charter::FILE_READER_NAME = "CFR";

const char* Charter::TYPE = "Charter";

/******************************************************************************
 CHARTER PUBLIC FUNCTIONS
 ******************************************************************************/

/*--------------------------------------------------------------------------------------
  -- Constructor  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
Charter::Charter(CommandProcessor* cmd_proc, const char* obj_name, const char* _outq_name, int _max_num_points): 
    CommandableObject(cmd_proc, obj_name, TYPE)
{
    /* Initialize Control Variables */
    pendingClose = false;

    /* Initialize Output Queue Name for Exporting */
    if(_outq_name != NULL)
    {
        SafeString tmp_outq_name(_outq_name);
        outQName = tmp_outq_name.getString(true);
    }
    else
    {
        outQName = NULL;
    }

    /* Initialize GUI Plot Parameters */
    numPlotPoints = _max_num_points;
    maxNumPoints = _max_num_points;
    offsetPlotPoints = 0;
    plotKey = 0;

    /* Initialize GUI */
    gdk_threads_enter();
    {
        /* Create Window */
        window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_container_set_border_width(GTK_CONTAINER(window), 10);
        gtk_window_set_default_size(GTK_WINDOW(window), WINDOW_X_SIZE_INIT, WINDOW_Y_SIZE_INIT);

        /* Create Plot Box*/
        plotRows = gtk_vbox_new(FALSE, 1);

        /* Create Number of Points Slider */
        numPlotPointsAdj = gtk_adjustment_new(0.0, 0.0, (double)maxNumPoints, 1.0, 0.0, 0.0);
        numPlotPointsScroll = gtk_hscrollbar_new (GTK_ADJUSTMENT(numPlotPointsAdj));
        gtk_range_set_update_policy(GTK_RANGE(numPlotPointsScroll), GTK_UPDATE_CONTINUOUS);
        numPlotPointsSpinner = gtk_spin_button_new(GTK_ADJUSTMENT(numPlotPointsAdj), 1.0, 0);
        GtkWidget* num_points_box = gtk_hbox_new(FALSE, 1);
        gtk_box_pack_start(GTK_BOX(num_points_box), numPlotPointsScroll, TRUE, TRUE, 1);
        gtk_box_pack_start(GTK_BOX(num_points_box), numPlotPointsSpinner, FALSE, FALSE, 5);
        GtkWidget* num_points_frame = gtk_frame_new("Number of Points");
        gtk_container_add(GTK_CONTAINER(num_points_frame), num_points_box);

        /* Create Offset of Points Slider */
        offsetPlotPointsAdj = gtk_adjustment_new(0.0, 0.0, (double)maxNumPoints, 1.0, 0.0, 0.0);
        offsetPlotPointsScroll = gtk_hscrollbar_new (GTK_ADJUSTMENT(offsetPlotPointsAdj));
        gtk_range_set_update_policy(GTK_RANGE(offsetPlotPointsScroll), GTK_UPDATE_CONTINUOUS);
        offsetPlotPointsSpinner = gtk_spin_button_new(GTK_ADJUSTMENT(offsetPlotPointsAdj), 1.0, 0);
        GtkWidget* offset_points_box = gtk_hbox_new(FALSE, 1);
        gtk_box_pack_start(GTK_BOX(offset_points_box), offsetPlotPointsScroll, TRUE, TRUE, 1);
        gtk_box_pack_start(GTK_BOX(offset_points_box), offsetPlotPointsSpinner, FALSE, FALSE, 5);
        GtkWidget* offset_points_frame = gtk_frame_new("Offset of Points");
        gtk_container_add(GTK_CONTAINER(offset_points_frame), offset_points_box);

        /* Create Settings Frame */
        lockCheck = gtk_check_button_new_with_label ("Lock Data");
        exportButton = gtk_button_new_with_label("Export");
        clearButton = gtk_button_new_with_label("Clear");
        GtkWidget* settings_box = gtk_hbox_new(FALSE, 1);
        gtk_box_pack_start(GTK_BOX(settings_box), lockCheck, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(settings_box), exportButton, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(settings_box), clearButton, FALSE, FALSE, 5);
        GtkWidget* settings_frame = gtk_frame_new("Settings");
        gtk_container_add(GTK_CONTAINER(settings_frame), settings_box);

        /* Create Control Box */
        GtkWidget* control_box = gtk_hbox_new(FALSE, 1);
        gtk_box_pack_start(GTK_BOX(control_box), settings_frame, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(control_box), num_points_frame, TRUE, TRUE, 5);
        gtk_box_pack_start(GTK_BOX(control_box), offset_points_frame, TRUE, TRUE, 5);

        /* Build Up Window */
        GtkWidget* window_box = gtk_vbox_new(FALSE, 1);
        gtk_box_pack_start(GTK_BOX(window_box), plotRows, TRUE, TRUE, 1);
        gtk_box_pack_start(GTK_BOX(window_box), control_box, FALSE, FALSE, 1);
        gtk_container_add(GTK_CONTAINER(window), window_box);

        /* Connect Signal Handlers */
        g_signal_connect(window,                "delete-event",     G_CALLBACK(deleteEvent),            this);
        g_signal_connect(numPlotPointsAdj,      "value-changed",    G_CALLBACK(numPointsHandler),       this);
        g_signal_connect(offsetPlotPointsAdj,   "value-changed",    G_CALLBACK(offsetPointsHandler),    this);
        g_signal_connect(lockCheck,             "clicked",          G_CALLBACK(lockHandler),            this);
        g_signal_connect(exportButton,          "clicked",          G_CALLBACK(exportHandler),          this);
        g_signal_connect(clearButton,           "clicked",          G_CALLBACK(clearHandler),           this);
    }
    gdk_threads_leave();

    /* Register Commands */
    registerCommand("SHOW",           (cmdFunc_t)&Charter::showChartCmd,      0,  "");
    registerCommand("HIDE",           (cmdFunc_t)&Charter::hideChartCmd,      0,  "");
    registerCommand("ADD_PLOT",       (cmdFunc_t)&Charter::addPlotCmd,       -1,  "<name> <inQ> [<sigificant digits>]");
    registerCommand("SET_PLOT_SIZE",  (cmdFunc_t)&Charter::setPlotPointsCmd,  1,  "<number of points to plot>");
}

/*--------------------------------------------------------------------------------------
  -- Destructor  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
Charter::~Charter(void)
{
    /* Delete Plots */
    DataPlot* plot;
    okey_t handle = plots.first(&plot);
    while(handle != plots.INVALID_KEY)
    {
        delete plot;
        plots.remove(handle);
        handle = plots.first(&plot);
    }

    /* Destroy Window */
    gdk_threads_enter();
    {
        gtk_widget_destroy(GTK_WIDGET(window));
    }
    gdk_threads_leave();
}

/*--------------------------------------------------------------------------------------
  -- createObject  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
CommandableObject* Charter::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    /* Parse Export Output Queue */
    const char* outq = StringLib::checkNullStr(argv[0]);

    /* Parse Max Number of Data Points */
    int max_num_points = MAX_DATA_POINTS;
    if(argc >= 2) max_num_points = (int)strtol(argv[1], NULL, 0);

    /* Create Charter */
    return new Charter(cmd_proc, name, outq, max_num_points);
}

/*--------------------------------------------------------------------------------------
  -- intAvg  - calculates a running average
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
double Charter::intAvg (int num, double curr_avg, double new_val)
{
    return ((curr_avg * (double)num) + new_val) / ((double)num + 1.0);
}

/*--------------------------------------------------------------------------------------
  -- setLabel  -
  --
  --   Notes: Not thread safe!!! must be called with thread protection
  -------------------------------------------------------------------------------------*/
void Charter::setLabel(GtkWidget* l, const char* fmt, ...)
{
    char lstr[32];
    va_list args;
    va_start(args, fmt);
    vsnprintf(lstr, 32, fmt, args);
    va_end(args);
    gtk_label_set_text(GTK_LABEL(l), lstr);
}

/*--------------------------------------------------------------------------------------
  -- getNumPoints  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
int Charter::getNumPoints(void)
{
    return numPlotPoints;
}

/*--------------------------------------------------------------------------------------
  -- getOffsetPoints  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
int Charter::getOffsetPoints(void)
{
    return offsetPlotPoints;
}

/*--------------------------------------------------------------------------------------
  -- setMarkers  -
  --
  --   Notes: Must be called in context of GTK!
  -------------------------------------------------------------------------------------*/
void Charter::setMarkers(marker_t marker, uint64_t key)
{
    DataPlot* plot;
    okey_t handle = plots.first(&plot);
    while(handle != plots.INVALID_KEY)
    {
        plot->setMarker(marker, key);
        handle = plots.next(&plot);
    }
}

/*--------------------------------------------------------------------------------------
  -- setZoom  -
  --
  --   Notes: Must be called in context of GTK!
  -------------------------------------------------------------------------------------*/
void Charter::setZoom(int start_index, int stop_index)
{
    /* Set Point Offset */
    offsetPlotPoints = MAX(MIN(start_index, maxNumPoints), 0);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(offsetPlotPointsAdj), offsetPlotPoints);

    /* Set Number of Points */
    numPlotPoints = MAX(MIN(stop_index - start_index, maxNumPoints), 0);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(numPlotPointsAdj), numPlotPoints);

    /* Replot */
    redrawPlots();
}

/******************************************************************************
 CHARTER PRIVATE FUNCTIONS (STATIC)
 ******************************************************************************/

/*--------------------------------------------------------------------------------------
  -- showChartCmd  -
  --
  --   Notes: Command Processor Command
  -------------------------------------------------------------------------------------*/
int Charter::showChartCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    gdk_threads_enter();
    {
        gtk_widget_show_all(window);
    }
    gdk_threads_leave();

    return 0;
}

/*--------------------------------------------------------------------------------------
  -- hideChartCmd  -
  --
  --   Notes: Command Processor Command
  -------------------------------------------------------------------------------------*/
int Charter::hideChartCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    gdk_threads_enter();
    {
        gtk_widget_hide_all(window);
    }
    gdk_threads_leave();

    return 0;
}

/*--------------------------------------------------------------------------------------
  -- addPlotCmd  -
  --
  --   Notes: * Command Processor Command
  --          * Currently this function is not thread safe, see below
  -------------------------------------------------------------------------------------*/
int Charter::addPlotCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    const char*   name        = argv[0];
    const char*   inq_name    = StringLib::checkNullStr(argv[1]);

    /* Check for optional Significant Digits */
    int sig_digits = 0;
    if(argc == 3)
    {
        sig_digits = strtol(argv[2], NULL, 0);
    }

    /* Check for Existing Name */
    DataPlot* old_plot;
    okey_t handle = plots.first(&old_plot);
    while(handle != plots.INVALID_KEY)
    {
        if(strcmp(old_plot->getName(), name) == 0)
        {
            mlog(CRITICAL, "Plot with same name already exists: %s\n", name);
            return -1;
        }
        handle = plots.next(&old_plot);
    }

    /* Check for Null Input Queue */
    if(!inq_name)
    {
        mlog(CRITICAL, "ERROR: chart must have an input queue\n");
        return -1;
    }

    /* Create New Plot */
    DataPlot* new_plot = new DataPlot(this, name, inq_name, outQName, maxNumPoints, sig_digits);
    gdk_threads_enter();
    {
        gtk_box_pack_start(GTK_BOX(plotRows), new_plot->getPlotBox(), TRUE, FALSE, 5);
    }
    gdk_threads_leave();
    plots.add(plotKey++, new_plot); // not thread safe

    return 0;
}

/*--------------------------------------------------------------------------------------
  -- setPlotPointsCmd  -
  --
  --   Notes: Command Processor Command
  -------------------------------------------------------------------------------------*/
int Charter::setPlotPointsCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    int num_plot_points = (int)strtol(argv[0], NULL, 0);

    if(num_plot_points < 0 || num_plot_points > maxNumPoints)
    {
        mlog(CRITICAL, "Number of plot points supplied is outside allowed bounds: %d\n", num_plot_points);
        return -1;
    }

    numPlotPoints = num_plot_points;
    gdk_threads_enter();
    {
        gtk_adjustment_set_value(GTK_ADJUSTMENT(numPlotPointsAdj), numPlotPoints);
        gtk_widget_queue_draw(GTK_WIDGET(numPlotPointsScroll));
    }
    gdk_threads_leave();

    return 0;
}

/*--------------------------------------------------------------------------------------
  -- deleteEvent  -
  -
  --   Notes: called in context of GTK Thread (no need to mutex class data)
  -------------------------------------------------------------------------------------*/
gboolean Charter::deleteEvent(GtkWidget* widget, GdkEvent* event, gpointer data)
{
    (void)widget;
    (void)event;

    Charter* charter = (Charter*)data;
    gtk_widget_hide_all(charter->window);

    if(charter->pendingClose == false)
    {
        charter->pendingClose = true;
        pthread_t pid;
        pthread_create(&pid, NULL, killThread, charter);
        pthread_detach(pid);
    }

    /* returning false destroys window as destroy event is issued;
     * therefore we need to return true to handle the delete ourselves */
    return TRUE;
}

/*--------------------------------------------------------------------------------------
  -- numPointsHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Charter::numPointsHandler(GtkAdjustment *adjustment, gpointer user_data)
{
    Charter* charter = (Charter*)user_data;

    int numpoints = (int)round(gtk_adjustment_get_value(adjustment));

    if(numpoints < 0)                       numpoints = 0;
    if(numpoints > charter->maxNumPoints)   numpoints = charter->maxNumPoints;

    charter->numPlotPoints = numpoints;
    charter->redrawPlots();
}

/*--------------------------------------------------------------------------------------
  -- offsetPointsHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Charter::offsetPointsHandler(GtkAdjustment *adjustment, gpointer user_data)
{
    Charter* charter = (Charter*)user_data;

    int offset = (int)round(gtk_adjustment_get_value(adjustment));

    if(offset < 0)                       offset = 0;
    if(offset > charter->maxNumPoints)   offset = charter->maxNumPoints;

    charter->offsetPlotPoints = offset;
    charter->redrawPlots();
}

/*--------------------------------------------------------------------------------------
  -- lockHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Charter::lockHandler(GtkButton* button, gpointer user_data)
{
    Charter* charter = (Charter*)user_data;

    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
    {
        DataPlot* plot;
        okey_t handle = charter->plots.first(&plot);
        while(handle != charter->plots.INVALID_KEY)
        {
            plot->lockData();
            handle = charter->plots.next(&plot);
        }
    }
    else
    {
        DataPlot* plot;
        okey_t handle = charter->plots.first(&plot);
        while(handle != charter->plots.INVALID_KEY)
        {
            plot->unlockData();
            handle = charter->plots.next(&plot);
        }
    }
}

/*--------------------------------------------------------------------------------------
  -- exportHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
#define EXPORT_RESPONSE_RANGE           1
#define EXPORT_RESPONSE_BLUE            2
#define EXPORT_RESPONSE_GREEN           3
#define EXPORT_RESPONSE_BLUE_W_STEP     4
#define EXPORT_RESPONSE_GREEN_W_STEP    5
#define EXPORT_RESPONSE_CANCEL          6

typedef struct {
    Charter* charter;
    int      function;
} export_button_t;

void Charter::exportHandler(GtkButton* button, gpointer user_data)
{
    (void)button;

    Charter* charter = (Charter*)user_data;

    /* Create Popup Window */
    GtkWidget* export_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER(export_window), 10);

    /* Create Button Box */
    GtkWidget* range_button     = gtk_button_new_with_label("Export Range");
    GtkWidget* blue_button      = gtk_button_new_with_label("Export Blue");
    GtkWidget* green_button     = gtk_button_new_with_label("Export Green");
    GtkWidget* bluestep_button  = gtk_button_new_with_label("Export Blue w/ Step");
    GtkWidget* greenstep_button = gtk_button_new_with_label("Export Green w/ Step");
    GtkWidget* button_box       = gtk_hbox_new(TRUE, 1);
    gtk_box_pack_start(GTK_BOX(button_box), range_button,       TRUE, TRUE, 1);
    gtk_box_pack_start(GTK_BOX(button_box), blue_button,        TRUE, TRUE, 1);
    gtk_box_pack_start(GTK_BOX(button_box), green_button,       TRUE, TRUE, 1);
    gtk_box_pack_start(GTK_BOX(button_box), bluestep_button,    TRUE, TRUE, 1);
    gtk_box_pack_start(GTK_BOX(button_box), greenstep_button,   TRUE, TRUE, 1);
    g_signal_connect(range_button,      "clicked", G_CALLBACK(exportRangeHandler),      charter);
    g_signal_connect(blue_button,       "clicked", G_CALLBACK(exportBlueHandler),       charter);
    g_signal_connect(green_button,      "clicked", G_CALLBACK(exportGreenHandler),      charter);
    g_signal_connect(bluestep_button,   "clicked", G_CALLBACK(exportBluestepHandler),   charter);
    g_signal_connect(greenstep_button,  "clicked", G_CALLBACK(exportGreenstepHandler),  charter);

    /* Build Export Box */
    GtkWidget* export_box = gtk_vbox_new(FALSE, 1);
    DataPlot* plot;
    okey_t handle = charter->plots.first(&plot);
    while(handle != charter->plots.INVALID_KEY)
    {
        GtkWidget* selection_label = gtk_label_new("---");
        Charter::setLabel(selection_label, "%s", plot->getName());
        GtkWidget* selected_check = gtk_check_button_new_with_label ("Selected");
        g_signal_connect(selected_check,  "clicked", G_CALLBACK(selectHandler), plot);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(selected_check), plot->selected);
        GtkWidget* selection_hbox = gtk_hbox_new(TRUE, 1);
        gtk_box_pack_start(GTK_BOX(selection_hbox), selection_label, TRUE, TRUE, 1);
        gtk_box_pack_start(GTK_BOX(selection_hbox), selected_check, TRUE, TRUE, 1);
        gtk_box_pack_start(GTK_BOX(export_box), selection_hbox, TRUE, TRUE, 1);
        handle = charter->plots.next(&plot);
    }
    gtk_box_pack_start(GTK_BOX(export_box), button_box, TRUE, TRUE, 1);

    /* Display Export Window */
    gtk_container_add (GTK_CONTAINER (export_window), export_box);
    gtk_widget_show_all (export_window);
}

/*--------------------------------------------------------------------------------------
  -- exportRangeHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Charter::exportRangeHandler(GtkButton* button, gpointer user_data)
{
    (void)button;

    Charter* charter = (Charter*)user_data;

    DataPlot* plot;
    okey_t handle = charter->plots.first(&plot);
    while(handle != charter->plots.INVALID_KEY)
    {
        plot->exportData();
        handle = charter->plots.next(&plot);
    }

    charter->redrawPlots();
}

/*--------------------------------------------------------------------------------------
  -- exportBlueHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Charter::exportBlueHandler(GtkButton* button, gpointer user_data)
{
    (void)button;

    Charter* charter = (Charter*)user_data;

    DataPlot* plot;
    okey_t handle = charter->plots.first(&plot);
    while(handle != charter->plots.INVALID_KEY)
    {
        plot->exportMarker(BLUE_MARKER, false);
        handle = charter->plots.next(&plot);
    }

    charter->redrawPlots();
}

/*--------------------------------------------------------------------------------------
  -- exportGreenHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Charter::exportGreenHandler(GtkButton* button, gpointer user_data)
{
    (void)button;

    Charter* charter = (Charter*)user_data;

    DataPlot* plot;
    okey_t handle = charter->plots.first(&plot);
    while(handle != charter->plots.INVALID_KEY)
    {
        plot->exportMarker(GREEN_MARKER, false);
        handle = charter->plots.next(&plot);
    }

    charter->redrawPlots();
}

/*--------------------------------------------------------------------------------------
  -- exportBluestepHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Charter::exportBluestepHandler(GtkButton* button, gpointer user_data)
{
    (void)button;

    Charter* charter = (Charter*)user_data;

    DataPlot* plot;
    okey_t handle = charter->plots.first(&plot);
    while(handle != charter->plots.INVALID_KEY)
    {
        plot->exportMarker(BLUE_MARKER, true);
        handle = charter->plots.next(&plot);
    }

    charter->redrawPlots();
}

/*--------------------------------------------------------------------------------------
  -- exportGreenstepHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Charter::exportGreenstepHandler(GtkButton* button, gpointer user_data)
{
    (void)button;

    Charter* charter = (Charter*)user_data;

    DataPlot* plot;
    okey_t handle = charter->plots.first(&plot);
    while(handle != charter->plots.INVALID_KEY)
    {
        plot->exportMarker(GREEN_MARKER, true);
        handle = charter->plots.next(&plot);
    }

    charter->redrawPlots();
}

/*--------------------------------------------------------------------------------------
  -- selectHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Charter::selectHandler(GtkButton* button, gpointer user_data)
{
    DataPlot* plot = (DataPlot*)user_data;

    plot->selected = (bool)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
}

/*--------------------------------------------------------------------------------------
  -- clearHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Charter::clearHandler(GtkButton* button, gpointer user_data)
{
    (void)button;

    Charter* charter = (Charter*)user_data;

    /* Set Offset to Zero */
    charter->offsetPlotPoints = 0;
    gtk_adjustment_set_value(GTK_ADJUSTMENT(charter->offsetPlotPointsAdj), charter->offsetPlotPoints);

    /* Clear Plots */
    DataPlot* plot;
    okey_t handle = charter->plots.first(&plot);
    while(handle != charter->plots.INVALID_KEY)
    {
        plot->clearData();
        plot->redraw();
        handle = charter->plots.next(&plot);
    }
}

/*--------------------------------------------------------------------------------------
  -- killThread  - exits the charter application in a thread safe way
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
void* Charter::killThread (void* parm)
{
    Charter* charter = (Charter*)parm;
    delete charter;
    return NULL;
}

/******************************************************************************
 CHARTER PRIVATE FUNCTIONS (NON-STATIC)
 ******************************************************************************/

/*--------------------------------------------------------------------------------------
  -- redrawPlots  -
  --
  --   Notes: This must be called in a gtk protected context
  -------------------------------------------------------------------------------------*/
void Charter::redrawPlots(void)
{
    DataPlot* plot;
    okey_t handle = plots.first(&plot);
    while(handle != plots.INVALID_KEY)
    {
        plot->redraw();
        handle = plots.next(&plot);
    }
}

/******************************************************************************
 DATAPLOT PUBLIC FUNCTIONS
 ******************************************************************************/

/*--------------------------------------------------------------------------------------
  -- Constructor  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
DataPlot::DataPlot(Charter* _charter, const char* _name, const char* inq_name, const char* outq_name, int max_num_points, int sig_digits)
{
    assert(inq_name);

    /* Initialize */
    charter = _charter;
    snprintf(name, PLOT_NAME_SIZE, "%s", _name);

    maxNumPoints = max_num_points;

    significantDigits = sig_digits;
    sprintf(valueFmt, "%%.%dlf", sig_digits);

    locked = false;
    lockedData = NULL;
    lockedKeys = NULL;
    lockedPkts = NULL;
    lockedSizes = NULL;

    selected = true;

    inQ = new Subscriber(inq_name);
    points = new MgOrdering<dp_t*>(NULL, NULL, max_num_points);

    outQ = NULL;
    if(outq_name != NULL)
    {
        outQ = new Publisher(outq_name);
    }

    /* Initialize Markers */
    showMarkers = false;
    memset(xBlue, 0, sizeof(xBlue));
    yBlue = 0.0;
    memset(xGreen, 0, sizeof(xGreen));
    yGreen = 0.0;

    /* Initialize Stats */
    memset(&viewStat, 0, sizeof(data_stat_t));
    memset(&totalStat, 0, sizeof(data_stat_t));
    viewStat.min = DBL_MAX;
    totalStat.min = DBL_MAX;

    /* Create Trace */
    gdk_threads_enter();
    {
        plotWidth = WINDOW_X_SIZE_INIT * 1.0;
        plotHeight = WINDOW_Y_SIZE_INIT * .50;

        xRange[0] = 0.0;
        xRange[1] = 1.0;
        yRange[0] = 0.0;
        yRange[1] = 1.0;

        /* Create Canvas */
        canvas = gtk_plot_canvas_new(plotWidth, plotHeight, 1.0);
        gtk_plot_canvas_grid_set_visible(GTK_PLOT_CANVAS(canvas), TRUE);
        gtk_widget_show(GTK_WIDGET(canvas));

        /* Create Plot */
        trace = gtk_plot_new(NULL);
        gtk_plot_hide_legends(GTK_PLOT(trace));
        gtk_plot_clip_data(GTK_PLOT(trace), TRUE);
        gtk_plot_set_transparent(GTK_PLOT(trace), TRUE);
        gtk_plot_axis_set_visible(gtk_plot_get_axis(GTK_PLOT(trace), GTK_PLOT_AXIS_TOP), FALSE);
        gtk_plot_axis_set_visible(gtk_plot_get_axis(GTK_PLOT(trace), GTK_PLOT_AXIS_RIGHT), FALSE);
        gtk_plot_axis_set_title(gtk_plot_get_axis(GTK_PLOT(trace), GTK_PLOT_AXIS_BOTTOM), "");
        gtk_plot_axis_set_labels_style(gtk_plot_get_axis(GTK_PLOT(trace), GTK_PLOT_AXIS_LEFT), GTK_PLOT_LABEL_FLOAT, significantDigits);
        gtk_plot_canvas_put_child(GTK_PLOT_CANVAS(canvas), gtk_plot_canvas_plot_new(GTK_PLOT(trace)), 0.1, 0.1, 0.9, 0.9);

        /* Create Dataset */
        dataset = GTK_PLOT_DATA(gtk_plot_data_new());
        gtk_plot_add_data(GTK_PLOT(trace), dataset);
        GdkColor plot_color;
        gdk_color_parse("red", &plot_color);
        gdk_color_alloc(gdk_colormap_get_system(), &plot_color);
        gtk_plot_data_set_symbol(dataset, GTK_PLOT_SYMBOL_NONE, GTK_PLOT_SYMBOL_EMPTY, 10, 2, &plot_color, &plot_color);
        gtk_plot_data_set_line_attributes(dataset, GTK_PLOT_LINE_SOLID, GDK_CAP_NOT_LAST, GDK_JOIN_MITER, 1.0, &plot_color);
        gtk_plot_data_set_connector(dataset, GTK_PLOT_CONNECT_STRAIGHT);
        gtk_widget_show(GTK_WIDGET(dataset));

        /* Create Blue Marker */
        blueMarker = gtk_plot_new(NULL);
        gtk_plot_hide_legends(GTK_PLOT(blueMarker));
        gtk_plot_clip_data(GTK_PLOT(blueMarker), TRUE);
        gtk_plot_set_transparent(GTK_PLOT(blueMarker), TRUE);
        gtk_plot_axis_set_visible(gtk_plot_get_axis(GTK_PLOT(blueMarker), GTK_PLOT_AXIS_TOP), FALSE);
        gtk_plot_axis_set_visible(gtk_plot_get_axis(GTK_PLOT(blueMarker), GTK_PLOT_AXIS_BOTTOM), FALSE);
        gtk_plot_axis_set_visible(gtk_plot_get_axis(GTK_PLOT(blueMarker), GTK_PLOT_AXIS_LEFT), FALSE);
        gtk_plot_axis_set_visible(gtk_plot_get_axis(GTK_PLOT(blueMarker), GTK_PLOT_AXIS_RIGHT), FALSE);
        gtk_plot_canvas_put_child(GTK_PLOT_CANVAS(canvas), gtk_plot_canvas_plot_new(GTK_PLOT(blueMarker)), 0.1, 0.1, 0.9, 0.9);
        blueDataset = GTK_PLOT_DATA(gtk_plot_data_new());
        gtk_plot_add_data(GTK_PLOT(blueMarker), blueDataset);
        GdkColor blue_color;
        gdk_color_parse("blue", &blue_color);
        gdk_color_alloc(gdk_colormap_get_system(), &blue_color);
        gtk_plot_data_set_symbol(blueDataset, GTK_PLOT_SYMBOL_DOT, GTK_PLOT_SYMBOL_EMPTY, 10, 2, &blue_color, &blue_color);
        gtk_plot_data_set_line_attributes(blueDataset, GTK_PLOT_LINE_SOLID, GDK_CAP_NOT_LAST, GDK_JOIN_MITER, 1.0, &blue_color);
        gtk_plot_data_set_connector(blueDataset, GTK_PLOT_CONNECT_STRAIGHT);
        gtk_plot_data_set_numpoints(blueDataset, 2); // this is hardcoded

        /* Create Blue Marker */
        greenMarker = gtk_plot_new(NULL);
        gtk_plot_hide_legends(GTK_PLOT(greenMarker));
        gtk_plot_clip_data(GTK_PLOT(greenMarker), TRUE);
        gtk_plot_set_transparent(GTK_PLOT(greenMarker), TRUE);
        gtk_plot_axis_set_visible(gtk_plot_get_axis(GTK_PLOT(greenMarker), GTK_PLOT_AXIS_TOP), FALSE);
        gtk_plot_axis_set_visible(gtk_plot_get_axis(GTK_PLOT(greenMarker), GTK_PLOT_AXIS_BOTTOM), FALSE);
        gtk_plot_axis_set_visible(gtk_plot_get_axis(GTK_PLOT(greenMarker), GTK_PLOT_AXIS_LEFT), FALSE);
        gtk_plot_axis_set_visible(gtk_plot_get_axis(GTK_PLOT(greenMarker), GTK_PLOT_AXIS_RIGHT), FALSE);
        gtk_plot_canvas_put_child(GTK_PLOT_CANVAS(canvas), gtk_plot_canvas_plot_new(GTK_PLOT(greenMarker)), 0.1, 0.1, 0.9, 0.9);
        greenDataset = GTK_PLOT_DATA(gtk_plot_data_new());
        gtk_plot_add_data(GTK_PLOT(greenMarker), greenDataset);
        GdkColor green_color;
        gdk_color_parse("green", &green_color);
        gdk_color_alloc(gdk_colormap_get_system(), &green_color);
        gtk_plot_data_set_symbol(greenDataset, GTK_PLOT_SYMBOL_DOT, GTK_PLOT_SYMBOL_EMPTY, 10, 2, &green_color, &green_color);
        gtk_plot_data_set_line_attributes(greenDataset, GTK_PLOT_LINE_SOLID, GDK_CAP_NOT_LAST, GDK_JOIN_MITER, 1.0, &green_color);
        gtk_plot_data_set_connector(greenDataset, GTK_PLOT_CONNECT_STRAIGHT);
        gtk_plot_data_set_numpoints(greenDataset, 2); // this is hardcoded

        /* Create View Statistics Info */
        GtkWidget* view_min_frame = gtk_frame_new("min");
        viewMinLabel = gtk_label_new("---");
        gtk_container_add(GTK_CONTAINER(view_min_frame), viewMinLabel);
        GtkWidget* view_max_frame = gtk_frame_new("max");
        viewMaxLabel = gtk_label_new("---");
        gtk_container_add(GTK_CONTAINER(view_max_frame), viewMaxLabel);
        GtkWidget* view_avg_frame = gtk_frame_new("avg");
        viewAvgLabel = gtk_label_new("---");
        gtk_container_add(GTK_CONTAINER(view_avg_frame), viewAvgLabel);
        GtkWidget* view_num_frame = gtk_frame_new("num");
        viewNumLabel = gtk_label_new("---");
        gtk_container_add(GTK_CONTAINER(view_num_frame), viewNumLabel);
        GtkWidget* view_stat_hbox = gtk_hbox_new(FALSE, 1);
        gtk_box_pack_start(GTK_BOX(view_stat_hbox), view_min_frame, TRUE, TRUE, 1);
        gtk_box_pack_start(GTK_BOX(view_stat_hbox), view_max_frame, TRUE, TRUE, 1);
        gtk_box_pack_start(GTK_BOX(view_stat_hbox), view_avg_frame, TRUE, TRUE, 1);
        gtk_box_pack_start(GTK_BOX(view_stat_hbox), view_num_frame, TRUE, TRUE, 1);

        /* Create Total Statistics Info */
        GtkWidget* total_min_frame = gtk_frame_new("total min");
        totalMinLabel = gtk_label_new("---");
        gtk_container_add(GTK_CONTAINER(total_min_frame), totalMinLabel);
        GtkWidget* total_max_frame = gtk_frame_new("total max");
        totalMaxLabel = gtk_label_new("---");
        gtk_container_add(GTK_CONTAINER(total_max_frame), totalMaxLabel);
        GtkWidget* total_avg_frame = gtk_frame_new("total avg");
        totalAvgLabel = gtk_label_new("---");
        gtk_container_add(GTK_CONTAINER(total_avg_frame), totalAvgLabel);
        GtkWidget* total_num_frame = gtk_frame_new("total num");
        totalNumLabel = gtk_label_new("---");
        gtk_container_add(GTK_CONTAINER(total_num_frame), totalNumLabel);
        GtkWidget* total_stat_hbox = gtk_hbox_new(FALSE, 1);
        gtk_box_pack_start(GTK_BOX(total_stat_hbox), total_min_frame, TRUE, TRUE, 1);
        gtk_box_pack_start(GTK_BOX(total_stat_hbox), total_max_frame, TRUE, TRUE, 1);
        gtk_box_pack_start(GTK_BOX(total_stat_hbox), total_avg_frame, TRUE, TRUE, 1);
        gtk_box_pack_start(GTK_BOX(total_stat_hbox), total_num_frame, TRUE, TRUE, 1);

        /* Create Marker Labels */
        GtkWidget* blue_key_frame = gtk_frame_new("blue key");
        blueKeyLabel = gtk_label_new("---");
        gtk_container_add(GTK_CONTAINER(blue_key_frame), blueKeyLabel);
        GtkWidget* blue_val_frame = gtk_frame_new("blue value");
        blueValLabel = gtk_label_new("---");
        gtk_container_add(GTK_CONTAINER(blue_val_frame), blueValLabel);
        GtkWidget* green_key_frame = gtk_frame_new("green key");
        greenKeyLabel = gtk_label_new("---");
        gtk_container_add(GTK_CONTAINER(green_key_frame), greenKeyLabel);
        GtkWidget* green_val_frame = gtk_frame_new("green value");
        greenValLabel = gtk_label_new("---");
        gtk_container_add(GTK_CONTAINER(green_val_frame), greenValLabel);
        GtkWidget* marker_hbox = gtk_hbox_new(FALSE, 1);
        gtk_box_pack_start(GTK_BOX(marker_hbox), blue_key_frame, TRUE, TRUE, 1);
        gtk_box_pack_start(GTK_BOX(marker_hbox), blue_val_frame, TRUE, TRUE, 1);
        gtk_box_pack_start(GTK_BOX(marker_hbox), green_key_frame, TRUE, TRUE, 1);
        gtk_box_pack_start(GTK_BOX(marker_hbox), green_val_frame, TRUE, TRUE, 1);

        /* Create Y-Scale Control */
        fixYMinCheck    = gtk_check_button_new_with_label ("Fix Y Min");
        fixYMaxCheck    = gtk_check_button_new_with_label ("Fix Y Max");
        scaleYMinAdj    = gtk_adjustment_new (0.0, 0.0, 10000000.0, 10.0, 0.0, 0.0);
        scaleYMaxAdj    = gtk_adjustment_new (0.0, 0.0, 10000000.0, 10.0, 0.0, 0.0);
        scaleYMinSpin   = gtk_spin_button_new (GTK_ADJUSTMENT(scaleYMinAdj), 10.0, significantDigits);
        scaleYMaxSpin   = gtk_spin_button_new (GTK_ADJUSTMENT(scaleYMaxAdj), 10.0, significantDigits);
        GtkWidget* fixy_hbox = gtk_hbox_new(FALSE, 1);
        gtk_box_pack_start(GTK_BOX(fixy_hbox), fixYMinCheck,    FALSE, FALSE, 1);
        gtk_box_pack_start(GTK_BOX(fixy_hbox), scaleYMinSpin,   FALSE, FALSE, 1);
        gtk_box_pack_start(GTK_BOX(fixy_hbox), fixYMaxCheck,    FALSE, FALSE, 1);
        gtk_box_pack_start(GTK_BOX(fixy_hbox), scaleYMaxSpin,   FALSE, FALSE, 1);

        /* Create Control Box */
        GtkWidget* control_vbox = gtk_vbox_new(FALSE, 1);
        gtk_box_pack_start(GTK_BOX(control_vbox), total_stat_hbox, FALSE, FALSE, 1);
        gtk_box_pack_start(GTK_BOX(control_vbox), view_stat_hbox, FALSE, FALSE, 1);
        gtk_box_pack_start(GTK_BOX(control_vbox), fixy_hbox, FALSE, FALSE, 1);
        gtk_box_pack_start(GTK_BOX(control_vbox), marker_hbox, FALSE, FALSE, 1);
        GtkWidget* control_frame = gtk_frame_new(name);
        gtk_container_add(GTK_CONTAINER(control_frame), control_vbox);

        /* Create Plot Row */
        plotBox = gtk_hbox_new(FALSE, 5);
        gtk_box_pack_start(GTK_BOX(plotBox), control_frame, TRUE, TRUE, 1);
        gtk_box_pack_start(GTK_BOX(plotBox), canvas, FALSE, FALSE, 1);

        /* Attach Signals */
        g_signal_connect(fixYMinCheck,   "clicked",                 G_CALLBACK(scaleYHandler),  this);
        g_signal_connect(fixYMaxCheck,   "clicked",                 G_CALLBACK(scaleYHandler),  this);
        g_signal_connect(scaleYMinAdj,   "value-changed",           G_CALLBACK(scaleYHandler),  this);
        g_signal_connect(scaleYMaxAdj,   "value-changed",           G_CALLBACK(scaleYHandler),  this);
        g_signal_connect(canvas,         "button-press-event",      G_CALLBACK(mouseHandler),   this);
        g_signal_connect(canvas,         "button-release-event",    G_CALLBACK(mouseHandler),   this);
        g_signal_connect(canvas,         "motion-notify-event",     G_CALLBACK(mouseHandler),   this);

        /* Add Trace to Window */
        gtk_widget_show_all(GTK_WIDGET(plotBox));
    }
    gdk_threads_leave();

    /* Create Data Mutex */
    pthread_mutex_init(&dataMut, NULL);

    /* Create Plot Signal */
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&drawMut, &attr);
    pthread_cond_init(&drawCond, NULL);

    /* Spawn Plot Threads */
    active = true;
    pthread_create(&dataPid, NULL, dataThread, this);
    pthread_create(&drawPid, NULL, drawThread, this);
}

/*--------------------------------------------------------------------------------------
  -- Destructor  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
DataPlot::~DataPlot(void)
{
    active = false;
    pthread_cond_signal(&drawCond);

    if(pthread_join(dataPid, NULL) != 0)
    {
        mlog(CRITICAL, "Unable to close data thread %s: %s\n", name, strerror(errno));
    }

    if(pthread_join(drawPid, NULL) != 0)
    {
        mlog(CRITICAL, "Unable to close draw thread %s: %s\n", name, strerror(errno));
    }
    
    delete points;
    delete inQ;
}

/*--------------------------------------------------------------------------------------
  -- redraw -
  --
  --   Notes: must be called in GTK context
  -------------------------------------------------------------------------------------*/
void DataPlot::redraw(void)
{
    pthread_cond_signal(&drawCond);
}

/*--------------------------------------------------------------------------------------
  -- lockData -
  --
  --   Notes: must be called in GTK context
  -------------------------------------------------------------------------------------*/
void DataPlot::lockData(void)
{
    pthread_mutex_lock(&dataMut);
    {
        /* Hide Markers */
        showMarkers = false;
        gtk_widget_hide(GTK_WIDGET(blueDataset));
        gtk_widget_hide(GTK_WIDGET(greenDataset));

        /* Lock Data */
        int numpoints = points->length();
        if(numpoints > 0)
        {
            lockedData = new double[numpoints];
            lockedKeys = new double[numpoints];
            lockedPkts = new unsigned char* [numpoints];
            lockedSizes = new int [numpoints];
            lockedPoints = numpoints;

            for(int i = 0; i < numpoints; i++)
            {
                lockedData[i] = (*points)[i]->value;
                lockedKeys[i] = (double)(*points)[i]->index;
                unsigned char* src = (unsigned char*)(((unsigned char*)(*points)[i]) + (*points)[i]->src_offset);
                lockedPkts[i] = src;
                lockedSizes[i] = (*points)[i]->size;
            }

            locked = true;
        }
    }
    pthread_mutex_unlock(&dataMut);
}

/*--------------------------------------------------------------------------------------
  -- unlockData -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
void DataPlot::unlockData(void)
{
    pthread_mutex_lock(&dataMut);
    {
        /* Hide Markers */
        showMarkers = false;
        gtk_widget_hide(GTK_WIDGET(blueDataset));
        gtk_widget_hide(GTK_WIDGET(greenDataset));

        /* Unlock Data */
        locked = false;

        if(lockedData != NULL)
        {
            delete [] lockedData;
        }

        if(lockedKeys != NULL)
        {
            delete [] lockedKeys;
        }

        if(lockedPkts != NULL)
        {
            delete [] lockedPkts;
        }

        if(lockedSizes != NULL)
        {
            delete [] lockedSizes;
        }

        lockedData = NULL;
        lockedKeys = NULL;
        lockedPkts = NULL;
        lockedSizes = NULL;
        lockedPoints = 0;
    }
    pthread_mutex_unlock(&dataMut);
}

/*--------------------------------------------------------------------------------------
  -- exportData -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
void DataPlot::exportData(void)
{
    if(locked && selected)
    {
        int start_index = MIN(blueIndex, greenIndex);
        int stop_index = MAX(blueIndex, greenIndex);
        if(start_index < lockedPoints && stop_index < lockedPoints)
        {
            for(int i = start_index; i <= stop_index; i++)
            {
                if(lockedPkts[i] != NULL)
                {
                    if(outQ != NULL)
                    {
                        outQ->postCopy(lockedPkts[i], lockedSizes[i]);
                    }
                }
            }
        }
    }
}

/*--------------------------------------------------------------------------------------
  -- exportData -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
void DataPlot::exportData(uint64_t key)
{
    int index = getLockedIndex(key);

    if(locked)
    {
        if(index >= 0 && index < lockedPoints)
        {
            if(outQ != NULL)
            {
                outQ->postCopy(lockedPkts[index], lockedSizes[index]);
            }
        }
    }
}

/*--------------------------------------------------------------------------------------
  -- exportData -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
void DataPlot::exportMarker(marker_t marker, bool with_increment)
{
    if(locked)
    {
        if(marker == BLUE_MARKER)
        {
            if(blueIndex >= 0 && blueIndex < lockedPoints)
            {
                if(outQ != NULL)
                {
                    outQ->postCopy(lockedPkts[blueIndex], lockedSizes[blueIndex]);
                }

                /* Increment */
                if(with_increment)
                {
                    incMarker(BLUE_MARKER);
                }
            }
        }
        else if(marker == GREEN_MARKER)
        {
            if(greenIndex >= 0 && greenIndex < lockedPoints)
            {
                if(outQ != NULL)
                {
                    outQ->postCopy(lockedPkts[greenIndex], lockedSizes[greenIndex]);
                }

                /* Increment */
                if(with_increment)
                {
                    incMarker(GREEN_MARKER);
                }
            }
        }
    }
}

/*--------------------------------------------------------------------------------------
  -- clearData -
  --
  --   Notes: Must be called in GTK context!
  -------------------------------------------------------------------------------------*/
void DataPlot::clearData (void)
{
    unlockData();

    pthread_mutex_lock(&dataMut);
    {
        /* Clear Structures */
        memset(xRange, 0, sizeof(xRange));
        memset(yRange, 0, sizeof(yRange));
        memset(xBlue, 0, sizeof(xBlue));
        memset(xGreen, 0, sizeof(xGreen));
        memset(&viewStat, 0, sizeof(viewStat));
        memset(&totalStat, 0, sizeof(totalStat));

        /* Clear Indices */
        yBlue = 0.0;
        yGreen = 0.0;
        blueIndex = 0;
        greenIndex = 0;

        /* Clear Point List */
        points->clear();
    }
    pthread_mutex_unlock(&dataMut);
}

/*--------------------------------------------------------------------------------------
  -- setMarker -
  --
  --   Notes: Must be called in GTK context!
  -------------------------------------------------------------------------------------*/
void DataPlot::setMarker(marker_t marker, uint64_t key)
{
    if(locked)
    {
        /* Show the Markers */
        showMarkers = true;
        gtk_widget_show(GTK_WIDGET(blueDataset));
        gtk_widget_show(GTK_WIDGET(greenDataset));

        /* Find Data Point */
        int index = getLockedIndex(key);

        /* Process the Button */
        if(marker == BLUE_MARKER)
        {
            blueIndex = index;
            xBlue[0] = lockedKeys[blueIndex];
            xBlue[1] = lockedKeys[blueIndex];
            yBlue = lockedData[blueIndex];
        }
        else if(marker == GREEN_MARKER)
        {
            greenIndex = index;
            xGreen[0] = lockedKeys[greenIndex];
            xGreen[1] = lockedKeys[greenIndex];
            yGreen = lockedData[greenIndex];
        }

        pthread_cond_signal(&drawCond);
    }
}

/*--------------------------------------------------------------------------------------
  -- setMarker -
  --
  --   Notes: Must be called in GTK context!
  -------------------------------------------------------------------------------------*/
void DataPlot::incMarker(marker_t marker)
{
    if(locked)
    {
        if(marker == BLUE_MARKER)
        {
            if(blueIndex < lockedPoints - 1)
            {
                blueIndex++;
            }
            xBlue[0] = lockedKeys[blueIndex];
            xBlue[1] = lockedKeys[blueIndex];
            yBlue = lockedData[blueIndex];
        }
        else if(marker == GREEN_MARKER)
        {
            if(greenIndex < lockedPoints - 1)
            {
                greenIndex++;
            }
            xGreen[0] = lockedKeys[greenIndex];
            xGreen[1] = lockedKeys[greenIndex];
            yGreen = lockedData[greenIndex];
        }
    }
}

/*--------------------------------------------------------------------------------------
  -- getLockedIndex -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
int DataPlot::getLockedIndex(uint64_t key)
{
    int index = 0;
    while(index < lockedPoints && lockedKeys[index] < key) index++;
    if(index > 0) index--;
    return index;
}

/*--------------------------------------------------------------------------------------
  -- getName -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
const char* DataPlot::getName(void)
{
    return name;
}

/*--------------------------------------------------------------------------------------
  -- getPlotBox -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
GtkWidget* DataPlot::getPlotBox(void)
{
    return plotBox;
}

/******************************************************************************
 DATAPLOT PRIVATE FUNCTIONS
 ******************************************************************************/

/*--------------------------------------------------------------------------------------
  -- dataThread  - data handling thread for DataPlot class
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
void* DataPlot::dataThread (void* parm)
{
    DataPlot* plot = (DataPlot*)parm;

    while(plot->active)
    {
        /* Block on read of message queue */
        Subscriber::msgRef_t ref;
        int status = plot->inQ->receiveRef(ref, SYS_TIMEOUT);
        if(status == MsgQ::STATE_OKAY)
        {
            dp_t* data_point = NULL; 
            try
            {
                RecordInterface recif((unsigned char*)ref.data, ref.size);
                if(recif.isRecordType(MetricRecord::rec_type)) data_point = (dp_t*)recif.getRecordData();
                else
                {
                    mlog(ERROR, "Unhandled record received by charter: %s\n", recif.getRecordType());
                }
            }
            catch (const InvalidRecordException& e)
            {
                mlog(CRITICAL, "Failed to parse serial data <%s> of size %d!\n", (unsigned char*)ref.data, ref.size);
                mlog(CRITICAL, "ERROR: %s\n", e.what());
            }
            
            if(data_point == NULL) continue;
            
            if(plot->locked == false)
            {
                pthread_mutex_lock(&plot->dataMut);
                {
                    /* Add Data */
                    plot->points->add(data_point->index, data_point);

                    /* Maintain Stats */
                    if(data_point->value < plot->totalStat.min) plot->totalStat.min = data_point->value;
                    if(data_point->value > plot->totalStat.max) plot->totalStat.max = data_point->value;
                    plot->totalStat.avg = Charter::intAvg(plot->totalStat.num, plot->totalStat.avg, data_point->value);
                    plot->totalStat.num++;

                    /* Dereference
                     *   data_point memory is freed later as
                     *   a part of MgOrdering freeing */
                    plot->inQ->dereference(ref, false);
                }
                pthread_mutex_unlock(&plot->dataMut);
            }
            else
            {
                /* Free Memory */
                plot->inQ->dereference(ref);
            }

            /* Signal Drawer */
            pthread_cond_signal(&plot->drawCond);
        }
    }

    return NULL;
}

/*--------------------------------------------------------------------------------------
  -- drawThread  - drawing thread for Charter class
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
void* DataPlot::drawThread (void* parm)
{
    assert(parm != NULL);

    DataPlot* plot = (DataPlot*)parm;

    double* xdata_buf = new double[plot->maxNumPoints];
    double* ydata_buf = new double[plot->maxNumPoints];

    while(plot->active)
    {
        /* Local Data */
        double *xdata = NULL, *ydata = NULL;
        double sum = 0.0, xmin = DBL_MAX, xmax = 0.0, ymin = DBL_MAX, ymax = 0.0;
        int numpoints = 0;

        /* Wait for Draw Signal */
        pthread_mutex_lock(&plot->drawMut);
        pthread_cond_wait(&plot->drawCond, &plot->drawMut);
        if(plot->active == false) break;

        /* Populate Data */
        pthread_mutex_lock(&plot->dataMut);
        {
            if(plot->locked == false)
            {
                int i;
                dp_t* point;

                /* Setup Variables */
                xdata = xdata_buf;
                ydata = ydata_buf;
                numpoints = plot->charter->getNumPoints();

                /* Go to First Point */
                i = 0;
                okey_t handle = plot->points->first(&point);
                while(handle != plot->points->INVALID_KEY && i < plot->charter->getOffsetPoints())
                {
                    handle = plot->points->next(&point);
                    i++;
                }

                /* Populate Data Buffers */
                i = 0;
                while(handle != plot->points->INVALID_KEY && i < numpoints)
                {
                    xdata[i] = (double)point->index;
                    ydata[i] = (double)point->value;

                    if(xdata[i] < xmin) xmin = xdata[i];
                    if(xdata[i] > xmax) xmax = xdata[i];
                    if(ydata[i] < ymin) ymin = ydata[i];
                    if(ydata[i] > ymax) ymax = ydata[i];

                    sum += ydata[i];

                    handle = plot->points->next(&point);
                    i++;
                }

                /* Recalculate Number of Points */
                numpoints = i;
            }
            else // data locked
            {
                int start_index = MIN(plot->lockedPoints - 1, plot->charter->getOffsetPoints());
                numpoints       = MIN(plot->lockedPoints - start_index, plot->charter->getNumPoints());

                xdata = &plot->lockedKeys[start_index];
                ydata = &plot->lockedData[start_index];

                for(int i = 0; i < numpoints; i++)
                {
                    if(xdata[i] < xmin) xmin = xdata[i];
                    if(xdata[i] > xmax) xmax = xdata[i];
                    if(ydata[i] < ymin) ymin = ydata[i];
                    if(ydata[i] > ymax) ymax = ydata[i];

                    sum += ydata[i];
                }
            }

            /* Calcuate Range */
            double yrange = (ymax - ymax) + 1.0;

            /* Save off Ranges and Stats */
            plot->viewStat.min = ymin;
            plot->viewStat.max = ymax;
            plot->viewStat.avg = sum / numpoints;
            plot->viewStat.num = numpoints;
            plot->xRange[0] = xmin;// - 0.05 * xrange;
            plot->xRange[1] = xmax;// + 0.05 * xrange;
            plot->yRange[0] = ymin - 0.25 * yrange;
            plot->yRange[1] = ymax + 0.25 * yrange;
        }
        pthread_mutex_unlock(&plot->dataMut);

        /* Populate Plot */
        gdk_threads_enter();
        {
            /* Y Minimum Scale Adjustments */
            if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(plot->fixYMinCheck)))
            {
                plot->yRange[0] = gtk_adjustment_get_value(GTK_ADJUSTMENT(plot->scaleYMinAdj));
            }

            /* Y Maximum Scale Adjustments */
            if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(plot->fixYMaxCheck)))
            {
                plot->yRange[1] = gtk_adjustment_get_value(GTK_ADJUSTMENT(plot->scaleYMaxAdj));
            }

            /* Get Temporary Value Format String */
            const char* valfmt = plot->valueFmt;

            /* Stats */
            Charter::setLabel(plot->totalMinLabel, valfmt, plot->totalStat.min);
            Charter::setLabel(plot->totalMaxLabel, valfmt, plot->totalStat.max);
            Charter::setLabel(plot->totalAvgLabel, valfmt, plot->totalStat.avg);
            Charter::setLabel(plot->totalNumLabel, "%d",   plot->totalStat.num);
            Charter::setLabel(plot->viewMinLabel,  valfmt, plot->viewStat.min);
            Charter::setLabel(plot->viewMaxLabel,  valfmt, plot->viewStat.max);
            Charter::setLabel(plot->viewAvgLabel,  valfmt, plot->viewStat.avg);
            Charter::setLabel(plot->viewNumLabel,  "%d",   plot->viewStat.num);

            /* Plot */
            gtk_plot_data_set_numpoints(plot->dataset, numpoints);
            gtk_plot_data_set_y(plot->dataset, ydata);
            gtk_plot_data_set_x(plot->dataset, xdata);
            gtk_plot_set_ticks(GTK_PLOT(plot->trace), GTK_PLOT_AXIS_X, round((plot->xRange[1] - plot->xRange[0]) * 0.1) + 1, 1);
            gtk_plot_set_ticks(GTK_PLOT(plot->trace), GTK_PLOT_AXIS_Y, (plot->yRange[1] - plot->yRange[0]) * 0.1, 1);
            gtk_plot_set_range(GTK_PLOT(plot->trace), plot->xRange[0], plot->xRange[1], plot->yRange[0], plot->yRange[1]);
            // --> very odd behavior seen where the set_range function needed to be called after the others or it would never return
            // --> the values going into the call were acceptable and within reasonable range

            if(plot->locked == true && plot->showMarkers == true)
            {
                /* Blue Marker */
                gtk_plot_set_range(GTK_PLOT(plot->blueMarker), plot->xRange[0], plot->xRange[1], plot->yRange[0], plot->yRange[1]);
                gtk_plot_data_set_y(plot->blueDataset, plot->yRange);
                gtk_plot_data_set_x(plot->blueDataset, plot->xBlue);
                Charter::setLabel(plot->blueKeyLabel, valfmt, plot->xBlue[0]);
                Charter::setLabel(plot->blueValLabel, valfmt, plot->yBlue);

                /* Green Marker */
                gtk_plot_set_range(GTK_PLOT(plot->greenMarker), plot->xRange[0], plot->xRange[1], plot->yRange[0], plot->yRange[1]);
                gtk_plot_data_set_y(plot->greenDataset, plot->yRange);
                gtk_plot_data_set_x(plot->greenDataset, plot->xGreen);
                Charter::setLabel(plot->greenKeyLabel, valfmt, plot->xGreen[0]);
                Charter::setLabel(plot->greenValLabel, valfmt, plot->yGreen);
            }

            /* Repaint */
            gtk_plot_canvas_paint(GTK_PLOT_CANVAS(plot->canvas));
            gtk_widget_queue_draw(GTK_WIDGET(plot->canvas));
        }
        gdk_threads_leave();
    }

    /* Clean Up */
    delete [] xdata_buf;
    delete [] ydata_buf;

    return NULL;
}

/*--------------------------------------------------------------------------------------
  -- scaleYHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void DataPlot::scaleYHandler(GtkButton* button, gpointer user_data)
{
    (void)button;

    DataPlot* plot = (DataPlot*)user_data;

    pthread_cond_signal(&plot->drawCond);
}

/*--------------------------------------------------------------------------------------
  -- resizeHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
gboolean DataPlot::resizeHandler(GtkWidget* widget, GtkAllocation* allocation, gpointer user_data)
{
    (void)widget;

    DataPlot* plot = (DataPlot*)user_data;
    plot->plotWidth = allocation->width;
    plot->plotHeight = allocation->height;
    gtk_plot_canvas_set_size(GTK_PLOT_CANVAS(plot->canvas), plot->plotWidth, plot->plotHeight);
    pthread_cond_signal(&plot->drawCond);

    return TRUE;
}

/*--------------------------------------------------------------------------------------
  -- mouseHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
#define LEFT_MOUSE_BUTTON   1
#define RIGHT_MOUSE_BUTTON  3
#define AXIS_OFFSET         0.10
#define ZOOM_MOVE_TOLERANCE 10
gboolean DataPlot::mouseHandler(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
    static int          pressed;
    static int          pressed_x;
    static int          pressed_y;
    static int          pressed_index;
    static uint64_t       pressed_key;

    DataPlot* plot = (DataPlot*)user_data;

    if(plot->locked == true)
    {
        /* Locate Key */
        double  x_axis_offset   = plot->plotWidth * AXIS_OFFSET;
        double  axis_width      = plot->plotWidth * (1.0 - (AXIS_OFFSET * 2));
        double  x_size          = plot->xRange[1] - plot->xRange[0];
        double  norm_x          = MAX(event->button.x - x_axis_offset, 0.0) / axis_width;
        int     x_index         = (int)lround(norm_x * x_size);
        uint64_t  marked_key      = (uint64_t)lround(plot->xRange[0]) + x_index;

        if(event->type == GDK_BUTTON_RELEASE)
        {
            pressed = event->type;

            /* Calculate Delta X */
            if(abs(x_index - pressed_index) > ZOOM_MOVE_TOLERANCE)
            {
                /* Process Drag - Zoom */
                int index1 = plot->getLockedIndex(marked_key);
                int index2 = plot->getLockedIndex(pressed_key);

                int start_index = MIN(index1, index2);
                int stop_index = MAX(index1, index2);

                plot->charter->setZoom(start_index, stop_index);
            }
            else
            {
                /* Process Click - Markers */
                if(event->button.button == LEFT_MOUSE_BUTTON)
                {
                    plot->charter->setMarkers(BLUE_MARKER, marked_key);
                }
                else if(event->button.button == RIGHT_MOUSE_BUTTON)
                {
                    plot->charter->setMarkers(GREEN_MARKER, marked_key);
                }
            }
        }
        else if(event->type == GDK_BUTTON_PRESS)
        {
            pressed = event->type;

            /* Save Last Event */
            pressed_x = event->button.x;
            pressed_y = event->button.y;
            pressed_index = x_index;
            pressed_key = marked_key;
        }
        else if(event->type == GDK_2BUTTON_PRESS)
        {
            pressed = event->type;

            /* Auto Export */
            plot->exportData(marked_key);
        }
        else if(event->type == GDK_MOTION_NOTIFY)
        {
            if(pressed == GDK_BUTTON_PRESS)
            {
                /* Draw Helper Line */
                gdk_draw_line(widget->window, widget->style->black_gc, pressed_x, pressed_y, event->button.x, pressed_y);
                gdk_draw_line(widget->window, widget->style->black_gc, pressed_x, pressed_y, event->button.x, pressed_y);
            }
        }
    }

    return TRUE;
}

