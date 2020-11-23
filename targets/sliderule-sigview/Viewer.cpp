/*-----------------------------------------------------------------------------
--
-- Module Name: Viewer.cpp
--
-- Description:
--
--   Encapsulation of GTK Viewer Application
--
-- Notes:
--
--      1. TODO: make the log message output plot_color coded (e.g. RED for errors)
--      2. TODO: text completion for command entry (completion group)
-----------------------------------------------------------------------------*/

/******************************************************************************
 INCLUDES
 ******************************************************************************/

#include <gtk/gtk.h>
#include <gtkextra/gtkextra.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <math.h>
#include <semaphore.h>

#include "core.h"
#include "ccsds.h"
#include "sigview.h"

#include "Viewer.h"

/******************************************************************************
 IMPORTED FUNCTION PROTOTYPES
 ******************************************************************************/

extern void console_quick_exit (int parm);

/******************************************************************************
 STATIC DATA
 ******************************************************************************/

const char* Viewer::TYPE = "Viewer";
const char* Viewer::PROTOCOL_LIST[NUM_PROTOCOLS] = {"ADASFILE", "ASCII", "BINARY", "SIS",    "ITOSARCH", "ADAS",   "NTGSE",  "DATASRV", "AOSFILE"};
const char* Viewer::FORMAT_LIST[NUM_PROTOCOLS]   = {"BINARY",   "ASCII", "BINARY", "BINARY", "BINARY",   "BINARY", "BINARY", "BINARY",  "BINARY"};
const char* Viewer::VIEWER_FILE_READER = "VFR";

/******************************************************************************
 PUBLIC FUNCTIONS (NON STATIC)
 ******************************************************************************/

/*--------------------------------------------------------------------------------------
  -- Constructor  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/

Viewer::Viewer(CommandProcessor* cmd_proc, const char* obj_name, const char* _dataq_name, const char* _scidataq_name, const char* _ttproc_name[NUM_PCES], const char* _reportproc_name, const char* _timeproc_name, const char* _ccsdsproc_name): 
    CommandableObject(cmd_proc, obj_name, TYPE)
{
    /* ---------- */
    /* Initialize */
    /* ---------- */

    /* Initialize Synchronization Variables */
    pthread_mutexattr_init(&bufmut_attr);
    pthread_mutexattr_settype(&bufmut_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&bufmut, &bufmut_attr);
    pthread_cond_init(&bufcond, NULL);

    pthread_mutexattr_init(&drawmut_attr);
    pthread_mutexattr_settype(&drawmut_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&drawmut, &drawmut_attr);
    pthread_cond_init(&drawcond, NULL);

    play_active = false;
    play_hz = 1.0;
    play_id = 0;

    /* Create Histogram Stream */
    assert(_dataq_name);
    recdataq = new Subscriber(_dataq_name);

    /* Initialize Raw Data Q Names */
    scidataq_name = StringLib::duplicate(_scidataq_name);
    if(scidataq_name == NULL)
    {
        scidataq_name = (char*)"";
    }

    /* Initialize Time Tag Processor Names */
    for(int i = 0; i < NUM_PCES; i++)
    {
        ttproc_name[i] = StringLib::duplicate(_ttproc_name[i]);
        if(ttproc_name[i] == NULL)
        {
            ttproc_name[i] = (char*)"";
        }
    }

    /* Initialize Report Processor Names */
    reportproc_name = StringLib::duplicate(_reportproc_name);
    if(reportproc_name == NULL)
    {
        reportproc_name = (char*)"";
    }

    /* Initialize Time Processor Names */
    timeproc_name = StringLib::duplicate(_timeproc_name);
    if(timeproc_name == NULL)
    {
        timeproc_name = (char*)"";
    }

    /* Initialize CCSDS Processor Names */
    ccsdsproc_name = StringLib::duplicate(_ccsdsproc_name);
    if(ccsdsproc_name == NULL)
    {
        ccsdsproc_name = (char*)"";
    }

    /* Initialize Parser List */
    memset(parser_qlist, 0, sizeof(parser_qlist));

    /* Initialize Auto Flush Count */
    autoflush_cnt = 0;

    /* Initialize HSTVS Interface */
    hstvs_name = NULL;
    hstvsq = NULL;

    /* Set Readers to NULL */
    file_reader = NULL;

    /* Initialize Plot Buffer */
    plot_buf_index = 0;
    plot_buf_max_size = DEFAULT_PLOT_BUF_MAX_SIZE;

    /* Initialize Latch Values */
    latch_active = false;
    latched_data_size = 0;
    autolatch_active = false;
    memset(autolatch_data_size, 0, sizeof(autolatch_data_size));
    autolatch_auto_peak_align = false;
    autolatch_x_offset = 0;
    autolatch_y_scale = 1.0;

    /* Initialize Plot Values */
    for(int i = 0; i < AtlasHistogram::MAX_HIST_SIZE; i++) plot_x_vals[i] = (double)i;
    bins_in_hist = (double)AtlasHistogram::MAX_HIST_SIZE;
    plot_action = PLOT_NORMAL;
    plot_zoom_level = 0;
    plot_empty_hists = true;
    plot_override_binsize = false;
    plot_fft = false;
    plot_accum = false;
    clear_accum = false;
    num_accum = 1;

    /* Initialize Display Options */
    display_utc = true;

    /* Register Commands */
    registerCommand("QUIT",                  (cmdFunc_t)&Viewer::quitCmd,              0, "");
    registerCommand("SET_PARSERS",           (cmdFunc_t)&Viewer::setParsersCmd,       -1, "<parser name matching protocol list>, ...");
    registerCommand("SET_PLAY_RATE",         (cmdFunc_t)&Viewer::setPlayRateCmd,       1, "<Hz>");
    registerCommand("SET_DATA_MODE",         (cmdFunc_t)&Viewer::setDataModeCmd,       1, "<STREAM|BUFFER|SAMPLE>");
    registerCommand("CLEAR_PLOTS",           (cmdFunc_t)&Viewer::clearPlotsCmd,        0, "");
    registerCommand("SET_PLOT_BUF_SIZE",     (cmdFunc_t)&Viewer::setPlotBufSizeCmd,    1, "<plot buffer maximum size>");
    registerCommand("SET_PLOT_EMPTY",        (cmdFunc_t)&Viewer::setPlotEmptyCmd,      1, "<TRUE|FALSE>");
    registerCommand("OVERRIDE_BINSIZE",      (cmdFunc_t)&Viewer::overrideBinsizeCmd,   1, "<binsize>");
    registerCommand("USE_PLOT_BINSIZE",      (cmdFunc_t)&Viewer::usePlotBinsizeCmd,    0, "");
    registerCommand("SET_PLOT_FFT",          (cmdFunc_t)&Viewer::setPlotFftCmd,        1, "<ENABLE|DISABLE>");
    registerCommand("AUTOLATCH_WAVEFORM",    (cmdFunc_t)&Viewer::setAutoWaveLatchCmd, -1, "<ENABLE|DISABLE> <wave subtype> <[AUTO]|bins to align> [<y_scale>]");
    registerCommand("ATTACH_HSTVS_Q",        (cmdFunc_t)&Viewer::attachHstvsCmdQCmd,   2, "<HSTVS name> <HSTVS command stream>");
    registerCommand("DISPLAY_UTC",           (cmdFunc_t)&Viewer::displayUtcCmd,        1, "<ENABLE|DISABLE>");

    /* ----------------------------- */
    /* Create General Use Components */
    /* ----------------------------- */

    font_desc = pango_font_description_from_string ("DejaVu Sans Mono");

    gdk_threads_enter();
    {
        /* ------------- */
        /* Create Panels */
        /* ------------- */

        GtkWidget* io_panel         = buildIOPanel();
        GtkWidget* selection_panel  = buildSelectionPanel();
        GtkWidget* control_panel    = buildControlPanel();
        GtkWidget* app_panel        = buildAppPanel();
        GtkWidget* settings_panel   = buildSettingsPanel();
        GtkWidget* filter_panel     = buildFilterPanel();
        GtkWidget* plot_tab         = buildPlotTab();
        GtkWidget* txstat_tab       = buildTxStatTab();
        GtkWidget* chstat1_tab      = buildChStatTab(0);
        GtkWidget* chstat2_tab      = buildChStatTab(1);
        GtkWidget* chstat3_tab      = buildChStatTab(2);
        GtkWidget* hstvs_tab        = buildHstvsTab();
        GtkWidget* analyze_tab      = buildAnalyzeTab();
        GtkWidget* time_tab         = buildTimeStatTab();
        GtkWidget* plot_panel       = buildPlotPanel();

        /* ------------- */
        /* Create Window */
        /* ------------- */

        /* Create Notebook */
        GtkWidget* info_notebook = gtk_notebook_new();
        gtk_notebook_append_page(GTK_NOTEBOOK(info_notebook), plot_tab,         gtk_label_new("Plot"));
        gtk_notebook_append_page(GTK_NOTEBOOK(info_notebook), txstat_tab,       gtk_label_new("TxStat"));
        gtk_notebook_append_page(GTK_NOTEBOOK(info_notebook), chstat1_tab,      gtk_label_new("ChStat1"));
        gtk_notebook_append_page(GTK_NOTEBOOK(info_notebook), chstat2_tab,      gtk_label_new("ChStat2"));
        gtk_notebook_append_page(GTK_NOTEBOOK(info_notebook), chstat3_tab,      gtk_label_new("ChStat3"));
        gtk_notebook_append_page(GTK_NOTEBOOK(info_notebook), hstvs_tab,        gtk_label_new("Hstvs"));
        gtk_notebook_append_page(GTK_NOTEBOOK(info_notebook), analyze_tab,      gtk_label_new("Analyze"));
        gtk_notebook_append_page(GTK_NOTEBOOK(info_notebook), time_tab,         gtk_label_new("Time"));

        /* Create Top Toolbar */
        GtkWidget* top_box = gtk_hbox_new(FALSE, 1);
        gtk_box_pack_start(GTK_BOX(top_box), io_panel,          FALSE,  TRUE, 1);
        gtk_box_pack_start(GTK_BOX(top_box), selection_panel,   TRUE,   TRUE, 1);
        gtk_box_pack_start(GTK_BOX(top_box), control_panel,     FALSE,  TRUE, 1);

        /* Create Plot Display */
        GtkWidget* left_box = gtk_vbox_new(FALSE, 1);
        gtk_box_pack_start(GTK_BOX(left_box), app_panel,      TRUE, TRUE, 1);
        gtk_box_pack_start(GTK_BOX(left_box), settings_panel, TRUE, TRUE, 1);
        gtk_box_pack_start(GTK_BOX(left_box), filter_panel,   TRUE, TRUE, 1);

        GtkWidget* right_box = gtk_vbox_new(FALSE, 1);
        plot_container = gtk_vbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(plot_container), plot_panel, TRUE, TRUE, 1);
        gtk_box_pack_start(GTK_BOX(right_box), plot_container, TRUE, TRUE, 1);
        gtk_box_pack_start(GTK_BOX(right_box), info_notebook, TRUE, TRUE, 1);

        GtkWidget* plot_box = gtk_hbox_new(FALSE, 1);
        gtk_box_pack_start(GTK_BOX(plot_box), left_box,  FALSE, TRUE, 1);
        gtk_box_pack_start(GTK_BOX(plot_box), right_box, TRUE, TRUE, 1);

        /* Create Window */
        window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_container_set_border_width(GTK_CONTAINER(window), 10);
        gtk_window_set_default_size(GTK_WINDOW(window), WINDOW_X_SIZE_INIT,WINDOW_Y_SIZE_INIT);

        /* Layout Window */
        GtkWidget* window_box = gtk_vbox_new(FALSE, 1);
        gtk_box_pack_start(GTK_BOX(window_box), top_box, FALSE, TRUE, 1);
        gtk_box_pack_start(GTK_BOX(window_box), plot_box, TRUE, TRUE, 1);
        gtk_container_add(GTK_CONTAINER(window), window_box);

        /* Show Widgets */
        gtk_widget_show_all(window);

        /* --------------- */
        /* Attach Handlers */
        /* --------------- */

        g_signal_connect(window,                    "delete-event",         G_CALLBACK(deleteEvent),           this);
        g_signal_connect(window,                    "destroy",              G_CALLBACK(appQuit),               this);
        g_signal_connect(refresh_button,            "clicked",              G_CALLBACK(refreshHandler),        this);
        g_signal_connect(restore_button,            "clicked",              G_CALLBACK(restoreHandler),        this);
        g_signal_connect(larrow_button,             "clicked",              G_CALLBACK(leftArrowHandler),      this);
        g_signal_connect(rarrow_button,             "clicked",              G_CALLBACK(rightArrowHandler),     this);
        g_signal_connect(fixx2spinner_check,        "clicked",              G_CALLBACK(fixXHandler),           this);
        g_signal_connect(fixx2rww_check,            "clicked",              G_CALLBACK(fixXHandler),           this);
        g_signal_connect(fixy2spinner_check,        "clicked",              G_CALLBACK(fixYHandler),           this);
        g_signal_connect(selector_adj,              "value-changed",        G_CALLBACK(selectorHandler),       this);
        g_signal_connect(play_button,               "clicked",              G_CALLBACK(playHandler),           this);
        g_signal_connect(stop_button,               "clicked",              G_CALLBACK(stopHandler),           this);
        g_signal_connect(open_button,               "clicked",              G_CALLBACK(fileOpenHandler),       this);
        g_signal_connect(export_button,             "clicked",              G_CALLBACK(fileExportHandler),     this);
        for(int p = 0; p < NUM_PCES; p++)
        {
            g_signal_connect(pcefilter[p],          "clicked",              G_CALLBACK(pceFilterHandler),      this);
        }
        g_signal_connect(plot_container,            "size-allocate",        G_CALLBACK(plotResizeHandler),     this);
        g_signal_connect(plot_canvas,               "button-press-event",   G_CALLBACK(plotMouseHandler),      this);
        g_signal_connect(plot_canvas,               "button-release-event", G_CALLBACK(plotMouseHandler),      this);
        g_signal_connect(plot_canvas,               "motion-notify-event",  G_CALLBACK(plotMouseHandler),      this);
        g_signal_connect(stream_radio,              "toggled",              G_CALLBACK(modeHandler),           this);
        g_signal_connect(buffer_radio,              "toggled",              G_CALLBACK(modeHandler),           this);
        g_signal_connect(sample_radio,              "toggled",              G_CALLBACK(modeHandler),           this);
        g_signal_connect(connection_button,         "clicked",              G_CALLBACK(connectionHandler),     this);
        g_signal_connect(latch_button,              "clicked",              G_CALLBACK(latchHandler),          this);
        g_signal_connect(hstvs_cmd_button,          "clicked",              G_CALLBACK(hstvsHandler),          this);
        g_signal_connect(plotfft_check,             "clicked",              G_CALLBACK(plotFFTHandler),        this);
        g_signal_connect(plotaccum_check,           "clicked",              G_CALLBACK(accumHandler),          this);
        g_signal_connect(clearaccum_button,         "clicked",              G_CALLBACK(clearAccumHandler),     this);
        g_signal_connect(intperiod_spinner_button,  "clicked",              G_CALLBACK(intPeriodHandler),      this);
        g_signal_connect(zoom_in_button,            "clicked",              G_CALLBACK(zoomInHandler),         this);
        g_signal_connect(zoom_out_button,           "clicked",              G_CALLBACK(zoomOutHandler),        this);
        g_signal_connect(autolatch_check,           "clicked",              G_CALLBACK(autolatchHandler),      this);
        g_signal_connect(fullcol_check,             "clicked",              G_CALLBACK(fullColHandler),        this);
        g_signal_connect(clearsig_button,           "clicked",              G_CALLBACK(reportstatClearHandler),this);
        g_signal_connect(flush_button,              "clicked",              G_CALLBACK(flushHandler),          this);
        g_signal_connect(autoset_clk_check,         "clicked",              G_CALLBACK(autoSetClkHandler),     this);
        g_signal_connect(cleartime_button,          "clicked",              G_CALLBACK(timestatClearHandler),  this);
    }
    gdk_threads_leave();

    /* -------------- */
    /* Create Threads */
    /* -------------- */

    /* Data Thread */
    pthread_t data_pid;
    pthread_create(&data_pid, NULL, dataThread, this);
    pthread_detach(data_pid);

    /* Plot Thread */
    pthread_t plot_pid;
    pthread_create(&plot_pid, NULL, plotThread, this);
    pthread_detach(plot_pid);

    /* Application Stat Thread */
    pthread_t appstat_pid;
    pthread_create(&appstat_pid, NULL, appstatThread, this);
    pthread_detach(appstat_pid);
}

/*--------------------------------------------------------------------------------------
  -- Destructor  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
Viewer::~Viewer(void)
{
}

/*--------------------------------------------------------------------------------------
  -- setDataMode  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
bool Viewer::setDataMode(data_mode_t mode)
{
    bool status;

    gdk_threads_enter();
    {
        switch(mode)
        {
            case STREAM_MODE:   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(stream_radio), TRUE); status = true; break;
            case BUFFER_MODE:   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(buffer_radio), TRUE); status = true; break;
            case SAMPLE_MODE:   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sample_radio), TRUE); status = true; break;
            default:            mlog(CRITICAL, "invalid mode selected: %d\n", mode); status = false; break;
        }
    }
    gdk_threads_leave();

    return status;
}

/*--------------------------------------------------------------------------------------
  -- setPlotBufSize  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
void Viewer::setPlotBufSize(int _plot_buf_max_size)
{
    plot_buf_max_size = _plot_buf_max_size;
}

/*--------------------------------------------------------------------------------------
  -- setPlotEmpty  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
void Viewer::setPlotEmpty(bool _plot_empty_hists)
{
    plot_empty_hists = _plot_empty_hists;
}

/*--------------------------------------------------------------------------------------
  -- overridePlotBinsize  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
void Viewer::overridePlotBinsize(double _binsize)
{
    plot_binsize = _binsize;
    plot_override_binsize = true;
}

/*--------------------------------------------------------------------------------------
  -- overridePlotBinsize  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
void Viewer::usePlotBinsize(void)
{
    plot_override_binsize = false;
}

/*--------------------------------------------------------------------------------------
  -- setPlotFFT  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
void Viewer::setPlotFFT(bool _enable)
{
    plot_fft = _enable;
}

/*--------------------------------------------------------------------------------------
  -- setPlotFFT  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
void Viewer::setAutoWaveLatch(bool _enable, bool _autoalign, int _alignment, double _scale, int _wave_subtype)
{
    gdk_threads_enter();
    {
        autolatch_active            = _enable;
        autolatch_wave_subtype      = _wave_subtype;
        autolatch_auto_peak_align   = _autoalign;
        autolatch_y_scale           = _scale;
        latch_active                = _enable;

        if(autolatch_auto_peak_align)
        {
            autolatch_x_offset = 0;
        }
        else
        {
            autolatch_x_offset = _alignment;
        }

        if(autolatch_active)
        {
            gtk_widget_show(GTK_WIDGET(latch_dataset));
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(autolatch_check), TRUE);
        }
        else
        {
            gtk_widget_hide(GTK_WIDGET(latch_dataset));
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(autolatch_check), FALSE);
        }
    }
    gdk_threads_leave();
}

/******************************************************************************
 PUBLIC FUNCTIONS (STATIC)
 ******************************************************************************/

/*--------------------------------------------------------------------------------------
  -- createObject  -
  -------------------------------------------------------------------------------------*/
CommandableObject* Viewer::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    const char* ttproc[NUM_PCES];

    const char* recdataq    = StringLib::checkNullStr(argv[0]);
    const char* sciq        = StringLib::checkNullStr(argv[1]);
                ttproc[0]   = StringLib::checkNullStr(argv[2]);
                ttproc[1]   = StringLib::checkNullStr(argv[3]);
                ttproc[2]   = StringLib::checkNullStr(argv[4]);
    const char* reportproc  = StringLib::checkNullStr(argv[5]);
    const char* timeproc    = StringLib::checkNullStr(argv[6]);
    const char* ccsdsproc   = StringLib::checkNullStr(argv[7]);

    if(recdataq == NULL)
    {
        mlog(CRITICAL, "Must supply a data queue for the viewer!\n");
        return NULL;
    }

    return new Viewer(cmd_proc, name, recdataq, sciq, ttproc, reportproc, timeproc, ccsdsproc);
}

/******************************************************************************
 PRIVATE FUNCTIONS
 ******************************************************************************/

/*--------------------------------------------------------------------------------------
  -- buildIOPanel  -
  -------------------------------------------------------------------------------------*/
GtkWidget* Viewer::buildIOPanel(void)
{
    open_button = gtk_button_new_with_label("OPEN");
    export_button = gtk_button_new_with_label("EXPORT");
    connection_button  = gtk_button_new_with_label("CON");

    GtkWidget* file_box = gtk_hbox_new(TRUE, 1);
    gtk_box_pack_start(GTK_BOX(file_box), open_button, TRUE, TRUE, 1);
    gtk_box_pack_start(GTK_BOX(file_box), export_button, TRUE, TRUE, 1);
    gtk_box_pack_start(GTK_BOX(file_box), connection_button, TRUE, TRUE, 1);

    GtkWidget* file_frame = gtk_frame_new("File");
    gtk_container_add(GTK_CONTAINER(file_frame), file_box);

    return file_frame;
}

/*--------------------------------------------------------------------------------------
  -- buildSelectionPanel  -
  -------------------------------------------------------------------------------------*/
GtkWidget* Viewer::buildSelectionPanel(void)
{
    selector_adj = gtk_adjustment_new (0.0, 0.0, (double)DEFAULT_PLOT_BUF_MAX_SIZE, 1.0, 0.0, 0.0);

    selector_slider = gtk_hscale_new (GTK_ADJUSTMENT(selector_adj));
    gtk_scale_set_digits(GTK_SCALE(selector_slider), 0);
    gtk_scale_set_value_pos(GTK_SCALE(selector_slider), GTK_POS_TOP);
    gtk_scale_set_draw_value(GTK_SCALE(selector_slider), TRUE);
    gtk_range_set_update_policy(GTK_RANGE(selector_slider), GTK_UPDATE_CONTINUOUS);

    numsel_label = gtk_label_new("0");

    GtkWidget* sel_box = gtk_hbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(sel_box), selector_slider, TRUE, TRUE, 1);
    gtk_box_pack_start(GTK_BOX(sel_box), numsel_label, FALSE, FALSE, 5);

    GtkWidget* selector_frame = gtk_frame_new("Selection");
    gtk_container_add(GTK_CONTAINER(selector_frame), sel_box);

    return selector_frame;
}

/*--------------------------------------------------------------------------------------
  -- buildControlPanel  -
  -------------------------------------------------------------------------------------*/
GtkWidget* Viewer::buildControlPanel(void)
{
    larrow_button   = gtk_button_new();
    stop_button     = gtk_button_new_from_stock(GTK_STOCK_MEDIA_STOP);
    play_button     = gtk_button_new_from_stock(GTK_STOCK_MEDIA_PLAY);
    rarrow_button   = gtk_button_new();
    refresh_button  = gtk_button_new_with_label("REFRESH");
    restore_button  = gtk_button_new_with_label("RESTORE");
    latch_button    = gtk_button_new_with_label("LATCH");

    GtkWidget* rarrow = gtk_arrow_new(GTK_ARROW_RIGHT, GTK_SHADOW_ETCHED_IN);
    GtkWidget* larrow = gtk_arrow_new(GTK_ARROW_LEFT, GTK_SHADOW_ETCHED_IN);

    gtk_container_add(GTK_CONTAINER(larrow_button), larrow);
    gtk_container_add(GTK_CONTAINER(rarrow_button), rarrow);

    GtkWidget* control_box = gtk_hbox_new(TRUE, 1);
    gtk_box_pack_start(GTK_BOX(control_box), larrow_button, TRUE, TRUE, 1);
    gtk_box_pack_start(GTK_BOX(control_box), stop_button, TRUE, TRUE, 1);
    gtk_box_pack_start(GTK_BOX(control_box), play_button, TRUE, TRUE, 1);
    gtk_box_pack_start(GTK_BOX(control_box), rarrow_button, TRUE, TRUE, 1);
    gtk_box_pack_start(GTK_BOX(control_box), refresh_button, TRUE, TRUE, 1);
    gtk_box_pack_start(GTK_BOX(control_box), restore_button, TRUE, TRUE, 1);
    gtk_box_pack_start(GTK_BOX(control_box), latch_button, TRUE, TRUE, 1);

    GtkWidget* control_frame = gtk_frame_new("Control");
    gtk_container_add(GTK_CONTAINER(control_frame), control_box);

    return control_frame;
}

/*--------------------------------------------------------------------------------------
  -- buildAppPanel  -
  -------------------------------------------------------------------------------------*/
GtkWidget* Viewer::buildAppPanel(void)
{
    GtkWidget* app_frame_status = gtk_frame_new("Status");
    app_textbuf_status = gtk_text_buffer_new(NULL);
    app_textview_status = gtk_text_view_new_with_buffer(GTK_TEXT_BUFFER(app_textbuf_status));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app_textview_status), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(app_textview_status), FALSE);
    gtk_widget_modify_font (app_textview_status, font_desc);
    gtk_container_add(GTK_CONTAINER(app_frame_status), app_textview_status);

    return app_frame_status;
}

/*--------------------------------------------------------------------------------------
  -- buildSettingsPanel  -
  -------------------------------------------------------------------------------------*/
GtkWidget* Viewer::buildSettingsPanel(void)
{
    stream_radio = gtk_radio_button_new_with_label (NULL, "strm");
    buffer_radio = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (stream_radio), "buff");
    sample_radio = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (buffer_radio), "smpl");

    GtkWidget* mode_box  = gtk_hbox_new (TRUE, 1);
    gtk_box_pack_start (GTK_BOX (mode_box), stream_radio, TRUE, TRUE, 1);
    gtk_box_pack_start (GTK_BOX (mode_box), buffer_radio, TRUE, TRUE, 1);
    gtk_box_pack_start (GTK_BOX (mode_box), sample_radio, TRUE, TRUE, 1);

    fixx2spinner_check          = gtk_check_button_new_with_label ("Fix X");
    fixy2spinner_check          = gtk_check_button_new_with_label ("Fix Y");
    fixx2rww_check              = gtk_check_button_new_with_label ("Fix RWW");
    scalex_adj                  = gtk_adjustment_new (bins_in_hist, 0.0, bins_in_hist, ceil(bins_in_hist/100.0), 0.0, 0.0);
    scaley_adj                  = gtk_adjustment_new (0.0, 0.0, 10000000.0, 10000.0, 0.0, 0.0);
    GtkWidget* scalex_spinner   = gtk_spin_button_new (GTK_ADJUSTMENT(scalex_adj), 100.0, 0);
    GtkWidget* scaley_spinner   = gtk_spin_button_new (GTK_ADJUSTMENT(scaley_adj), 100.0, 0);

    GtkWidget* fixx_hbox = gtk_hbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(fixx_hbox), fixx2spinner_check, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(fixx_hbox), scalex_spinner, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(fixx_hbox), fixx2rww_check, FALSE, FALSE, 1);

    GtkWidget* fixy_hbox = gtk_hbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(fixy_hbox), fixy2spinner_check, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(fixy_hbox), scaley_spinner, FALSE, FALSE, 1);

    GtkWidget* settings_panel = gtk_vbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(settings_panel), mode_box, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(settings_panel), fixx_hbox, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(settings_panel), fixy_hbox, FALSE, FALSE, 1);

    GtkWidget* settings_frame = gtk_frame_new("Settings");
    gtk_container_add(GTK_CONTAINER(settings_frame), settings_panel);

    return settings_frame;
}

/*--------------------------------------------------------------------------------------
  -- buildFilterPanel  -
  -------------------------------------------------------------------------------------*/
GtkWidget* Viewer::buildFilterPanel(void)
{
    GtkWidget* pce_vbox[NUM_PCES];
    GtkWidget* filter_hbox = gtk_hbox_new(TRUE, 1);
    char clabel[8] = {'\0'};

    for(int p = 0; p < NUM_PCES; p++)
    {
        pce_vbox[p] = gtk_vbox_new(TRUE, 1);
        sprintf(clabel, "pce%d", p+1);
        pcefilter[p] = gtk_check_button_new_with_label (clabel);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pcefilter[p]), TRUE);
        gtk_box_pack_start(GTK_BOX(pce_vbox[p]), pcefilter[p], TRUE, TRUE, 1);
        for(int i = 0; i < AtlasHistogram::NUM_TYPES; i++)
        {
            pktfilter[p][i] = gtk_check_button_new_with_label (AtlasHistogram::type2str((AtlasHistogram::type_t)i));
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pktfilter[p][i]), TRUE);
            gtk_box_pack_start(GTK_BOX(pce_vbox[p]), pktfilter[p][i], TRUE, TRUE, 1);
        }
        gtk_box_pack_start(GTK_BOX(filter_hbox), pce_vbox[p], TRUE, TRUE, 1);
     }

    GtkWidget* filter_frame = gtk_frame_new("Filter");
    gtk_container_add(GTK_CONTAINER(filter_frame), filter_hbox);

    return filter_frame;
}

/*--------------------------------------------------------------------------------------
  -- buildPlotTab  -
  -------------------------------------------------------------------------------------*/
GtkWidget* Viewer::buildPlotTab(void)
{
    GtkWidget* plot_frame_type = gtk_frame_new("type");
    plot_label_type = gtk_label_new("nill");
    gtk_container_add(GTK_CONTAINER(plot_frame_type), plot_label_type);
    GtkWidget* plot_frame_pce = gtk_frame_new("pce");
    plot_label_pce = gtk_label_new("nill");
    gtk_container_add(GTK_CONTAINER(plot_frame_pce), plot_label_pce);
    GtkWidget* plot_frame_binsize = gtk_frame_new("binsize(ns)");
    plot_label_binsize = gtk_label_new("nill");
    gtk_container_add(GTK_CONTAINER(plot_frame_binsize), plot_label_binsize);
    GtkWidget* plot_frame_histsize = gtk_frame_new("histsize");
    plot_label_histsize = gtk_label_new("nill");
    gtk_container_add(GTK_CONTAINER(plot_frame_histsize), plot_label_histsize);
    GtkWidget* plot_frame_mfpavail = gtk_frame_new("mfp");
    plot_label_mfpavail = gtk_label_new("nill");
    gtk_container_add(GTK_CONTAINER(plot_frame_mfpavail), plot_label_mfpavail);
    GtkWidget* plot_frame_mfc = gtk_frame_new("mfc");
    plot_label_mfc = gtk_label_new("nill");
    gtk_container_add(GTK_CONTAINER(plot_frame_mfc), plot_label_mfc);
    GtkWidget* plot_frame_utc = gtk_frame_new("utc");
    plot_label_utc = gtk_label_new("nill");
    gtk_container_add(GTK_CONTAINER(plot_frame_utc), plot_label_utc);
    GtkWidget* plot_frame_rws = gtk_frame_new("rws(clk)");
    plot_label_rws = gtk_label_new("nill");
    gtk_container_add(GTK_CONTAINER(plot_frame_rws), plot_label_rws);
    GtkWidget* plot_frame_rww = gtk_frame_new("rww(clk)");
    plot_label_rww = gtk_label_new("nill");
    gtk_container_add(GTK_CONTAINER(plot_frame_rww), plot_label_rww);
    GtkWidget* plot_frame_numtx = gtk_frame_new("numtx");
    plot_label_numtx = gtk_label_new("nill");
    gtk_container_add(GTK_CONTAINER(plot_frame_numtx), plot_label_numtx);
    GtkWidget* plot_frame_intperiod = gtk_frame_new("intperiod(shots)");
    plot_label_intperiod = gtk_label_new("nill");
    gtk_container_add(GTK_CONTAINER(plot_frame_intperiod), plot_label_intperiod);
    GtkWidget* plot_frame_mbps = gtk_frame_new("Mbps");
    plot_label_mbps = gtk_label_new("nill");
    gtk_container_add(GTK_CONTAINER(plot_frame_mbps), plot_label_mbps);

    GtkWidget* plot_frame_signal = gtk_frame_new("signal");
    plot_textbuf_signal = gtk_text_buffer_new(NULL);
    GtkWidget* plot_textview_signal = gtk_text_view_new_with_buffer(GTK_TEXT_BUFFER(plot_textbuf_signal));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(plot_textview_signal), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(plot_textview_signal), FALSE);
    gtk_widget_modify_font (plot_textview_signal, font_desc);
    gtk_container_add(GTK_CONTAINER(plot_frame_signal), plot_textview_signal);

    GtkWidget* plot_frame_meta = gtk_frame_new("histogram meta data");
    plot_textbuf_meta = gtk_text_buffer_new(NULL);
    GtkWidget* plot_textview_meta = gtk_text_view_new_with_buffer(GTK_TEXT_BUFFER(plot_textbuf_meta));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(plot_textview_meta), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(plot_textview_meta), FALSE);
    gtk_widget_modify_font (plot_textview_meta, font_desc);
    gtk_container_add(GTK_CONTAINER(plot_frame_meta), plot_textview_meta);

    GtkWidget* plot_frame_channels = gtk_frame_new("channel statistics");
    plot_textbuf_channels = gtk_text_buffer_new(NULL);
    GtkWidget* plot_textview_channels = gtk_text_view_new_with_buffer(GTK_TEXT_BUFFER(plot_textbuf_channels));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(plot_textview_channels), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(plot_textview_channels), FALSE);
    gtk_widget_modify_font (plot_textview_channels, font_desc);
    gtk_container_add(GTK_CONTAINER(plot_frame_channels), plot_textview_channels);

    GtkWidget* plot_frame_errors = gtk_frame_new("ancillary");
    plot_textbuf_ancillary = gtk_text_buffer_new(NULL);
    GtkWidget* plot_textview_errors = gtk_text_view_new_with_buffer(GTK_TEXT_BUFFER(plot_textbuf_ancillary));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(plot_textview_errors), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(plot_textview_errors), FALSE);
    gtk_widget_modify_font (plot_textview_errors, font_desc);
    gtk_container_add(GTK_CONTAINER(plot_frame_errors), plot_textview_errors);

    GtkWidget* plot_frame_dlbs = gtk_frame_new("downlink band statistics");
    plot_textbuf_dlbs = gtk_text_buffer_new(NULL);
    GtkWidget* plot_textview_dlbs = gtk_text_view_new_with_buffer(GTK_TEXT_BUFFER(plot_textbuf_dlbs));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(plot_textview_dlbs), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(plot_textview_dlbs), FALSE);
    gtk_widget_modify_font (plot_textview_dlbs, font_desc);
    gtk_container_add(GTK_CONTAINER(plot_frame_dlbs), plot_textview_dlbs);

    GtkWidget* plot_frame_stats = gtk_frame_new("packet statistics");
    plot_textbuf_stats = gtk_text_buffer_new(NULL);
    GtkWidget* plot_textview_stats = gtk_text_view_new_with_buffer(GTK_TEXT_BUFFER(plot_textbuf_stats));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(plot_textview_stats), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(plot_textview_stats), FALSE);
    gtk_widget_modify_font (plot_textview_stats, font_desc);
    gtk_container_add(GTK_CONTAINER(plot_frame_stats), plot_textview_stats);

    GtkWidget* plot_table = gtk_table_new(6, 8, TRUE);

    gtk_table_attach(GTK_TABLE(plot_table), plot_frame_type,      0, 1, 0, 1, GTK_FILL, GTK_FILL, 1, 1);
    gtk_table_attach(GTK_TABLE(plot_table), plot_frame_pce,       1, 2, 0, 1, GTK_FILL, GTK_FILL, 1, 1);
    gtk_table_attach(GTK_TABLE(plot_table), plot_frame_mfc,       0, 1, 1, 2, GTK_FILL, GTK_FILL, 1, 1);
    gtk_table_attach(GTK_TABLE(plot_table), plot_frame_utc,       1, 2, 1, 2, GTK_FILL, GTK_FILL, 1, 1);
    gtk_table_attach(GTK_TABLE(plot_table), plot_frame_binsize,   0, 1, 2, 3, GTK_FILL, GTK_FILL, 1, 1);
    gtk_table_attach(GTK_TABLE(plot_table), plot_frame_histsize,  1, 2, 2, 3, GTK_FILL, GTK_FILL, 1, 1);
    gtk_table_attach(GTK_TABLE(plot_table), plot_frame_rws,       0, 1, 3, 4, GTK_FILL, GTK_FILL, 1, 1);
    gtk_table_attach(GTK_TABLE(plot_table), plot_frame_rww,       1, 2, 3, 4, GTK_FILL, GTK_FILL, 1, 1);
    gtk_table_attach(GTK_TABLE(plot_table), plot_frame_mfpavail,  0, 1, 4, 5, GTK_FILL, GTK_FILL, 1, 1);
    gtk_table_attach(GTK_TABLE(plot_table), plot_frame_numtx,     1, 2, 4, 5, GTK_FILL, GTK_FILL, 1, 1);
    gtk_table_attach(GTK_TABLE(plot_table), plot_frame_intperiod, 0, 1, 5, 6, GTK_FILL, GTK_FILL, 1, 1);
    gtk_table_attach(GTK_TABLE(plot_table), plot_frame_mbps,      1, 2, 5, 6, GTK_FILL, GTK_FILL, 1, 1);

    gtk_table_attach(GTK_TABLE(plot_table), plot_frame_signal,    2, 4, 0, 3, GTK_FILL, GTK_FILL, 1, 1);
    gtk_table_attach(GTK_TABLE(plot_table), plot_frame_meta,      2, 4, 3, 6, GTK_FILL, GTK_FILL, 1, 1);
    gtk_table_attach(GTK_TABLE(plot_table), plot_frame_channels,  4, 6, 0, 3, GTK_FILL, GTK_FILL, 1, 1);
    gtk_table_attach(GTK_TABLE(plot_table), plot_frame_errors,    4, 6, 3, 6, GTK_FILL, GTK_FILL, 1, 1);
    gtk_table_attach(GTK_TABLE(plot_table), plot_frame_dlbs,      6, 8, 0, 3, GTK_FILL, GTK_FILL, 1, 1);
    gtk_table_attach(GTK_TABLE(plot_table), plot_frame_stats,     6, 8, 3, 6, GTK_FILL, GTK_FILL, 1, 1);

    return plot_table;
}

/*--------------------------------------------------------------------------------------
  -- buildTxStatPanel  -
  -------------------------------------------------------------------------------------*/
GtkWidget* Viewer::buildTxStatTab(void)
{
    GtkWidget* txstat_table = gtk_table_new(6, 8, TRUE);

    for(int i = 0; i < NUM_PCES; i++)
    {
        GtkWidget* txstat_frame_statcnt = gtk_frame_new("statcnt");
        txstat_label_statcnt[i] = gtk_label_new("nill");
        gtk_container_add(GTK_CONTAINER(txstat_frame_statcnt), txstat_label_statcnt[i]);

        GtkWidget* txstat_frame_txcnt = gtk_frame_new("txcnt");
        txstat_label_txcnt[i] = gtk_label_new("nill");
        gtk_container_add(GTK_CONTAINER(txstat_frame_txcnt), txstat_label_txcnt[i]);

        GtkWidget* txstat_frame_mindelta = gtk_frame_new("mindelta");
        txstat_label_mindelta[i] = gtk_label_new("nill");
        gtk_container_add(GTK_CONTAINER(txstat_frame_mindelta), txstat_label_mindelta[i]);
        GtkWidget* txstat_frame_maxdelta = gtk_frame_new("maxdelta");
        txstat_label_maxdelta[i] = gtk_label_new("nill");
        gtk_container_add(GTK_CONTAINER(txstat_frame_maxdelta), txstat_label_maxdelta[i]);
        GtkWidget* txstat_frame_avgdelta = gtk_frame_new("avgdelta");
        txstat_label_avgdelta[i] = gtk_label_new("nill");
        gtk_container_add(GTK_CONTAINER(txstat_frame_avgdelta), txstat_label_avgdelta[i]);

        txstat_button_clear[i] = gtk_button_new_with_label("clear");
        g_signal_connect(txstat_button_clear[i], "clicked", G_CALLBACK(txstatClearHandler), this);

        GtkWidget* txstat_frame_taginfo = gtk_frame_new("tags");
        txstat_textbuf_taginfo[i] = gtk_text_buffer_new(NULL);
        GtkWidget* txstat_textview_taginfo = gtk_text_view_new_with_buffer(GTK_TEXT_BUFFER(txstat_textbuf_taginfo[i]));
        gtk_text_view_set_editable(GTK_TEXT_VIEW(txstat_textview_taginfo), FALSE);
        gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(txstat_textview_taginfo), FALSE);
        gtk_widget_modify_font (txstat_textview_taginfo, font_desc);
        gtk_container_add(GTK_CONTAINER(txstat_frame_taginfo), txstat_textview_taginfo);

        gtk_table_attach(GTK_TABLE(txstat_table), txstat_frame_statcnt,     0+(i*3), 1+(i*3), 0, 1, GTK_FILL, GTK_FILL, 1, 1);
        gtk_table_attach(GTK_TABLE(txstat_table), txstat_frame_txcnt,       0+(i*3), 1+(i*3), 1, 2, GTK_FILL, GTK_FILL, 1, 1);
        gtk_table_attach(GTK_TABLE(txstat_table), txstat_frame_mindelta,    0+(i*3), 1+(i*3), 2, 3, GTK_FILL, GTK_FILL, 1, 1);
        gtk_table_attach(GTK_TABLE(txstat_table), txstat_frame_maxdelta,    0+(i*3), 1+(i*3), 3, 4, GTK_FILL, GTK_FILL, 1, 1);
        gtk_table_attach(GTK_TABLE(txstat_table), txstat_frame_avgdelta,    0+(i*3), 1+(i*3), 4, 5, GTK_FILL, GTK_FILL, 1, 1);
        gtk_table_attach(GTK_TABLE(txstat_table), txstat_button_clear[i],   0+(i*3), 1+(i*3), 5, 6, GTK_FILL, GTK_FILL, 1, 1);
        gtk_table_attach(GTK_TABLE(txstat_table), txstat_frame_taginfo,     1+(i*3), 3+(i*3), 0, 6, GTK_FILL, GTK_FILL, 1, 1);
    }

    return txstat_table;
}

/*--------------------------------------------------------------------------------------
  -- buildChStatPanel  -
  -------------------------------------------------------------------------------------*/
GtkWidget* Viewer::buildChStatTab(int pce)
{
    GtkWidget* clear_box = gtk_hbox_new(TRUE, 1);
    for(int i = 0; i < NUM_CHANNELS + 1; i++)
    {
        char chstr[5];
        if(i == 0) sprintf(chstr, "all");
        else       sprintf(chstr, "%d", i);
        chstat_button_clear[pce][i] = gtk_button_new_with_label(chstr);
        g_signal_connect(chstat_button_clear[pce][i], "clicked", G_CALLBACK(chstatClearHandler), this);
        gtk_box_pack_start(GTK_BOX(clear_box), chstat_button_clear[pce][i], TRUE, TRUE, 1);
    }

    chstat_textbuf_info[pce] = gtk_text_buffer_new(NULL);
    GtkWidget* chstat_textview = gtk_text_view_new_with_buffer(GTK_TEXT_BUFFER(chstat_textbuf_info[pce]));
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(chstat_textview), FALSE);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(chstat_textview), FALSE);
    gtk_widget_modify_font (chstat_textview, font_desc);
    GtkWidget* chstat_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (chstat_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(chstat_window), chstat_textview);

    GtkWidget* chstat_frame = gtk_frame_new("channel stats");
    gtk_container_add(GTK_CONTAINER(chstat_frame), chstat_window);
    GtkWidget* clear_frame = gtk_frame_new("clear stats");
    gtk_container_add(GTK_CONTAINER(clear_frame), clear_box);

    GtkWidget* chstat_box = gtk_vbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(chstat_box), chstat_frame, TRUE,  TRUE, 1);
    gtk_box_pack_start(GTK_BOX(chstat_box), clear_frame,  FALSE, TRUE, 1);

    return chstat_box;
}

/*--------------------------------------------------------------------------------------
  -- buildHstvsTab  -
  -------------------------------------------------------------------------------------*/
GtkWidget* Viewer::buildHstvsTab(void)
{
    GtkWidget* hstvs_box = gtk_hbox_new(FALSE, 1);

    for(int i = 0; i < NUM_RX_PER_TX; i++)
    {
        GtkWidget* range_label = gtk_label_new("Range:");
        hstvs_range_buf[i] = gtk_entry_buffer_new ("4500667", -1);
        GtkWidget* range_entry = gtk_entry_new_with_buffer (hstvs_range_buf[i]);

        GtkWidget* pe_label = gtk_label_new("PE:");
        if(i == 0)  hstvs_pe_buf[i] = gtk_entry_buffer_new ("1.0", -1);
        else        hstvs_pe_buf[i] = gtk_entry_buffer_new ("0.0", -1);
        GtkWidget* pe_entry = gtk_entry_new_with_buffer (hstvs_pe_buf[i]);

        GtkWidget* width_label = gtk_label_new("Width:");
        hstvs_width_buf[i] = gtk_entry_buffer_new ("10.0", -1);
        GtkWidget* width_entry = gtk_entry_new_with_buffer (hstvs_width_buf[i]);

        char frame_name[16]; memset(frame_name, 0, 16);
        sprintf(frame_name, "return #%d\n", i+1);
        GtkWidget* return_frame = gtk_frame_new(frame_name);

        GtkWidget* label_box = gtk_vbox_new(FALSE, 1);
        gtk_box_pack_start(GTK_BOX(label_box), range_label, TRUE, FALSE, 1);
        gtk_box_pack_start(GTK_BOX(label_box), pe_label,    TRUE, FALSE, 1);
        gtk_box_pack_start(GTK_BOX(label_box), width_label, TRUE, FALSE, 1);

        GtkWidget* entry_box = gtk_vbox_new(FALSE, 1);
        gtk_box_pack_start(GTK_BOX(entry_box), range_entry, TRUE, FALSE, 1);
        gtk_box_pack_start(GTK_BOX(entry_box), pe_entry,    TRUE, FALSE, 1);
        gtk_box_pack_start(GTK_BOX(entry_box), width_entry, TRUE, FALSE, 1);

        GtkWidget* return_box = gtk_hbox_new(FALSE, 1);
        gtk_box_pack_start(GTK_BOX(return_box), label_box,  TRUE, FALSE, 1);
        gtk_box_pack_start(GTK_BOX(return_box), entry_box,  TRUE, FALSE, 1);
        gtk_container_add(GTK_CONTAINER(return_frame), return_box);

        gtk_box_pack_start(GTK_BOX(hstvs_box), return_frame, TRUE, TRUE, 1);
    }

    GtkWidget* noise_label = gtk_label_new("Noise:");
    hstvs_noise_buf = gtk_entry_buffer_new ("1000000.0", -1);
    GtkWidget* noise_entry = gtk_entry_new_with_buffer (hstvs_noise_buf);
    GtkWidget* noise_box = gtk_hbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(noise_box), noise_label,   TRUE, TRUE, 1);
    gtk_box_pack_start(GTK_BOX(noise_box), noise_entry,   TRUE, TRUE, 1);

    hstvs_strong_check = gtk_check_button_new_with_label ("Strong");
    hstvs_weak_check = gtk_check_button_new_with_label ("Weak");
    GtkWidget* spot_box = gtk_hbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(spot_box), hstvs_strong_check, TRUE, TRUE, 1);
    gtk_box_pack_start(GTK_BOX(spot_box), hstvs_weak_check,   TRUE, TRUE, 1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hstvs_strong_check), true);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hstvs_weak_check), true);

    hstvs_cmd_button = gtk_button_new_with_label("SEND COMMAND");

    GtkWidget* misc_frame = gtk_frame_new("misc");
    GtkWidget* misc_box = gtk_vbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(misc_box), noise_box,    TRUE, TRUE, 1);
    gtk_box_pack_start(GTK_BOX(misc_box), spot_box,     TRUE, TRUE, 1);
    gtk_container_add(GTK_CONTAINER(misc_frame), misc_box);

    GtkWidget* right_box = gtk_vbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(right_box), misc_frame,          TRUE, TRUE, 1);
    gtk_box_pack_start(GTK_BOX(right_box), hstvs_cmd_button,    TRUE, TRUE, 1);

    gtk_box_pack_start(GTK_BOX(hstvs_box), right_box,   TRUE, TRUE, 1);

    return hstvs_box;
}

/*--------------------------------------------------------------------------------------
  -- buildAnalyzeTab  -
  -------------------------------------------------------------------------------------*/
GtkWidget* Viewer::buildAnalyzeTab(void)
{
    GtkWidget* tabbox               = gtk_hbox_new(FALSE, 1);

    GtkWidget* toolbox              = gtk_vbox_new(FALSE, 1);
    GtkWidget* spacervbox           = gtk_vbox_new(FALSE, 1);

    plotfft_check                   = gtk_check_button_new_with_label("Fourier Transform");

    GtkWidget* accum_hbox           = gtk_hbox_new(FALSE, 1);
    plotaccum_check                 = gtk_check_button_new_with_label("Accumulate");
    clearaccum_button               = gtk_button_new_with_label("Clear Accumulation");
    gtk_box_pack_start(GTK_BOX(accum_hbox), plotaccum_check, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(accum_hbox), clearaccum_button, FALSE, FALSE, 1);

    GtkWidget* intperiod_hbox       = gtk_hbox_new(FALSE, 1);
    intperiod_spinner_button        = gtk_button_new_with_label ("Set Integration Period");
    intperiod_adj                   = gtk_adjustment_new (50.0, 1.0, 500.0, 1.0, 0.0, 0.0);
    GtkWidget* intperiod_spinner    = gtk_spin_button_new (GTK_ADJUSTMENT(intperiod_adj), 1.0, 0);
    gtk_box_pack_start(GTK_BOX(intperiod_hbox), intperiod_spinner_button, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(intperiod_hbox), intperiod_spinner, FALSE, FALSE, 1);

    GtkWidget* zoom_hbox            = gtk_hbox_new(FALSE, 1);
    zoom_in_button                  = gtk_button_new_with_label ("Zoom In");
    zoom_out_button                 = gtk_button_new_with_label ("Zoom Out");
    gtk_box_pack_start(GTK_BOX(zoom_hbox), zoom_in_button, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(zoom_hbox), zoom_out_button, FALSE, FALSE, 1);

    autolatch_check                 = gtk_check_button_new_with_label ("AutoLatch BCE");
    fullcol_check                   = gtk_check_button_new_with_label ("Full Column");
    clearsig_button                 = gtk_button_new_with_label("Clear Signal Stats");
    flush_button                    = gtk_button_new_with_label("Flush Science Data");

    gtk_box_pack_start(GTK_BOX(toolbox), plotfft_check, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(toolbox), accum_hbox, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(toolbox), intperiod_hbox, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(toolbox), zoom_hbox, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(toolbox), autolatch_check, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(toolbox), fullcol_check, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(toolbox), clearsig_button, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(toolbox), flush_button, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(toolbox), spacervbox, TRUE, TRUE, 1);

    GtkWidget* analysis_frame = gtk_frame_new("analysis display");
    analysis_textbuf = gtk_text_buffer_new(NULL);
    GtkWidget* analysis_textview = gtk_text_view_new_with_buffer(GTK_TEXT_BUFFER(analysis_textbuf));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(analysis_textview), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(analysis_textview), FALSE);
    gtk_widget_modify_font (analysis_textview, font_desc);
    gtk_container_add(GTK_CONTAINER(analysis_frame), analysis_textview);

    GtkWidget* current_frame = gtk_frame_new("current display");
    current_textbuf = gtk_text_buffer_new(NULL);
    GtkWidget* current_textview = gtk_text_view_new_with_buffer(GTK_TEXT_BUFFER(current_textbuf));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(current_textview), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(current_textview), FALSE);
    gtk_widget_modify_font (current_textview, font_desc);
    gtk_container_add(GTK_CONTAINER(current_frame), current_textview);

    GtkWidget* textbuf_vbox = gtk_vbox_new(FALSE, 1);
    gtk_box_pack_start(GTK_BOX(textbuf_vbox), analysis_frame, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(textbuf_vbox), current_frame, FALSE, FALSE, 1);

    gtk_box_pack_start(GTK_BOX(tabbox), toolbox, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(tabbox), textbuf_vbox, TRUE, TRUE, 1);

    return tabbox;
}

/*--------------------------------------------------------------------------------------
  -- buildTimeStatTab  -
  -------------------------------------------------------------------------------------*/
GtkWidget* Viewer::buildTimeStatTab(void)
{
    GtkWidget* tabbox               = gtk_hbox_new(FALSE, 1);

    GtkWidget* toolbox              = gtk_vbox_new(FALSE, 1);
    GtkWidget* spacervbox           = gtk_vbox_new(FALSE, 1);

    autoset_clk_check               = gtk_check_button_new_with_label("AutoSet Ruler Clock");
    cleartime_button                = gtk_button_new_with_label("Clear Time Stats");

    gtk_box_pack_start(GTK_BOX(toolbox), autoset_clk_check, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(toolbox), cleartime_button, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(toolbox), spacervbox, TRUE, TRUE, 1);

    GtkWidget* time_frame = gtk_frame_new("time display");
    time_textbuf = gtk_text_buffer_new(NULL);
    GtkWidget* time_textview = gtk_text_view_new_with_buffer(GTK_TEXT_BUFFER(time_textbuf));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(time_textview), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(time_textview), FALSE);
    gtk_widget_modify_font (time_textview, font_desc);
    gtk_container_add(GTK_CONTAINER(time_frame), time_textview);

    gtk_box_pack_start(GTK_BOX(tabbox), toolbox, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(tabbox), time_frame, TRUE, TRUE, 1);

    return tabbox;
}

/*--------------------------------------------------------------------------------------
  -- buildPlotCavnas  -
  -------------------------------------------------------------------------------------*/
GtkWidget* Viewer::buildPlotPanel(void)
{
    gint page_width = plot_width = WINDOW_X_SIZE_INIT * .80;
    gint page_height = plot_height = WINDOW_Y_SIZE_INIT * .80;

    plot_x_range[0] = 0.0;
    plot_x_range[1] = 1.0;
    plot_y_range[0] = 0.0;
    plot_y_range[1] = 1.0;

    plot_canvas = gtk_plot_canvas_new(page_width, page_height, 1.0);
    gtk_plot_canvas_grid_set_visible(GTK_PLOT_CANVAS(plot_canvas), TRUE);
    gtk_widget_show(GTK_WIDGET(plot_canvas));

    /* Create Histogram Plot */

    hist_plot = gtk_plot_new(NULL);
    gtk_plot_hide_legends(GTK_PLOT(hist_plot));
    gtk_plot_clip_data(GTK_PLOT(hist_plot), TRUE);
    gtk_plot_set_transparent(GTK_PLOT(hist_plot), TRUE);
    x_axis = gtk_plot_get_axis(GTK_PLOT(hist_plot), GTK_PLOT_AXIS_BOTTOM);
    y_axis = gtk_plot_get_axis(GTK_PLOT(hist_plot), GTK_PLOT_AXIS_LEFT);
    gtk_plot_axis_set_visible(gtk_plot_get_axis(GTK_PLOT(hist_plot), GTK_PLOT_AXIS_TOP), FALSE);
    gtk_plot_axis_set_visible(gtk_plot_get_axis(GTK_PLOT(hist_plot), GTK_PLOT_AXIS_RIGHT), FALSE);

    gtk_plot_canvas_put_child(GTK_PLOT_CANVAS(plot_canvas), gtk_plot_canvas_plot_new(GTK_PLOT(hist_plot)), 0.07, 0.07, 0.93, 0.93);

    plot_dataset = GTK_PLOT_DATA(gtk_plot_data_new());
    gtk_plot_add_data(GTK_PLOT(hist_plot), plot_dataset);
    GdkColor plot_color;
    gdk_color_parse("red", &plot_color);
    gdk_color_alloc(gdk_colormap_get_system(), &plot_color);
    gtk_plot_data_set_symbol(plot_dataset, GTK_PLOT_SYMBOL_DOT, GTK_PLOT_SYMBOL_EMPTY, 10, 2, &plot_color, &plot_color);
    gtk_plot_data_set_line_attributes(plot_dataset, GTK_PLOT_LINE_SOLID, GDK_CAP_NOT_LAST, GDK_JOIN_MITER, 1.0, &plot_color);
    gtk_plot_data_set_connector(plot_dataset, GTK_PLOT_CONNECT_STRAIGHT);
    gtk_widget_show(GTK_WIDGET(plot_dataset));

    /* Create Latch Plot */

    latch_plot = gtk_plot_new(NULL);
    gtk_plot_hide_legends(GTK_PLOT(latch_plot));
    gtk_plot_clip_data(GTK_PLOT(latch_plot), TRUE);
    gtk_plot_set_transparent(GTK_PLOT(latch_plot), TRUE);
    gtk_plot_axis_set_visible(gtk_plot_get_axis(GTK_PLOT(latch_plot), GTK_PLOT_AXIS_TOP), FALSE);
    gtk_plot_axis_set_visible(gtk_plot_get_axis(GTK_PLOT(latch_plot), GTK_PLOT_AXIS_BOTTOM), FALSE);
    gtk_plot_axis_set_visible(gtk_plot_get_axis(GTK_PLOT(latch_plot), GTK_PLOT_AXIS_LEFT), FALSE);
    gtk_plot_axis_set_visible(gtk_plot_get_axis(GTK_PLOT(latch_plot), GTK_PLOT_AXIS_RIGHT), FALSE);

    gtk_plot_canvas_put_child(GTK_PLOT_CANVAS(plot_canvas), gtk_plot_canvas_plot_new(GTK_PLOT(latch_plot)), 0.07, 0.07, 0.93, 0.93);

    latch_dataset = GTK_PLOT_DATA(gtk_plot_data_new());
    gtk_plot_add_data(GTK_PLOT(latch_plot), latch_dataset);
    GdkColor latch_color;
    gdk_color_parse("blue", &latch_color);
    gdk_color_alloc(gdk_colormap_get_system(), &latch_color);
    gtk_plot_data_set_symbol(latch_dataset, GTK_PLOT_SYMBOL_DOT, GTK_PLOT_SYMBOL_EMPTY, 10, 2, &latch_color, &latch_color);
    gtk_plot_data_set_line_attributes(latch_dataset, GTK_PLOT_LINE_SOLID, GDK_CAP_NOT_LAST, GDK_JOIN_MITER, 1.0, &latch_color);
    gtk_plot_data_set_connector(latch_dataset, GTK_PLOT_CONNECT_STRAIGHT);

    return plot_canvas;
}

/*--------------------------------------------------------------------------------------
  -- clearPlots  -
  --
  --   Notes: Not thread safe!!! must be called with thread protection
  -------------------------------------------------------------------------------------*/
void Viewer::clearPlots(void)
{
    /* Clear Plot Buffers */
    plot_buf.clear();
    plot_buf_index = 0;

    /* Clear Plot Parameters */
    setLabel(numsel_label, "%d", 0);
    plot_action = PLOT_NORMAL;

    /* Signal Data and Plot Threads */
    pthread_cond_signal(&bufcond);
    pthread_cond_signal(&drawcond);
}

/*--------------------------------------------------------------------------------------
  -- setLabel  -
  --
  --   Notes: Not thread safe!!! must be called with thread protection
  -------------------------------------------------------------------------------------*/
void Viewer::setLabel(GtkWidget* l, const char* fmt, ...)
{
    char lstr[32];
    va_list args;
    va_start(args, fmt);
    vsnprintf(lstr, 32, fmt, args);
    va_end(args);
    gtk_label_set_text(GTK_LABEL(l), lstr);
}

/*--------------------------------------------------------------------------------------
  -- nextHist  -
  --
  --   Notes: Not thread safe!!! must be called with thread protection
  -------------------------------------------------------------------------------------*/
bool Viewer::nextHist(void)
{
    bool found_enabled_hist = false;
    while((!found_enabled_hist) && (plot_buf_index < (plot_buf.length() - 1)))
    {
        plot_buf_index++;

        view_hist_t* hist = plot_buf[plot_buf_index];
        if(plot_empty_hists == true || hist->sum > 0)
        {
            if(hist->pceNum < NUM_PCES && hist->type < AtlasHistogram::NUM_TYPES && hist->type >= 0)
            {
                if(hist->pceNum == ALL_PCE)
                {
                    for(int p = 0; p < NUM_PCES; p++)
                    {
                        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pktfilter[p][hist->type])))
                        {
                            found_enabled_hist = true;
                            break;
                        }
                    }
                }
                else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pktfilter[hist->pceNum][hist->type])))

                {
                    found_enabled_hist = true;
                }
            }
            else
            {
                mlog(CRITICAL, "invalid pce or histogram type: %d %d\n", hist->pceNum, hist->type);
                break;
            }
        }
    }

    if(found_enabled_hist)
    {
        plot_action = PLOT_NORMAL;
        pthread_cond_signal(&drawcond);
    }

    return found_enabled_hist;
}

/*--------------------------------------------------------------------------------------
  -- prevHist  -
  --
  --   Notes: Not thread safe!!! must be called with thread protection
  -------------------------------------------------------------------------------------*/
bool Viewer::prevHist(void)
{
    bool found_enabled_hist = false;
    while((!found_enabled_hist) && (plot_buf_index > 0))
    {
        plot_buf_index--;

        view_hist_t* hist = plot_buf[plot_buf_index];
        if(plot_empty_hists == true || hist->sum > 0)
        {
            if(hist->pceNum < NUM_PCES && hist->type < AtlasHistogram::NUM_TYPES && hist->type >= 0)
            {
                if(hist->pceNum == ALL_PCE)
                {
                    for(int p = 0; p < NUM_PCES; p++)
                    {
                        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pktfilter[p][hist->type])))
                        {
                            found_enabled_hist = true;
                            break;
                        }
                    }
                }
                else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pktfilter[hist->pceNum][hist->type])))

                {
                    found_enabled_hist = true;
                }
            }
            else
            {
                mlog(CRITICAL, "invalid pce or histogram type: %d %d\n", hist->pceNum, hist->type);
                break;
            }
        }
    }

    if(found_enabled_hist)
    {
        plot_action = PLOT_NORMAL;
        pthread_cond_signal(&drawcond);
    }

    return found_enabled_hist;
}

/*--------------------------------------------------------------------------------------
  -- histHandler  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
void Viewer::histHandler(view_hist_t* hist, int size)
{
    /* Populate Plot Buffer */
    gdk_threads_enter();
    {
        /* Handle Auto-Latching */
        if( (autolatch_active) &&
            (hist->type == AtlasHistogram::GRL) )
        {
            BceHistogram::bceHist_t* bcehist = (BceHistogram::bceHist_t*)hist;
            if(bcehist->subtype == autolatch_wave_subtype)
            {
                int pce = bcehist->hist.pceNum;
                int spot = bcehist->spot;
                int start_bin = MIN(MAX(autolatch_x_offset, 0), AtlasHistogram::MAX_HIST_SIZE);
                int end_bin = MAX(MIN(hist->size + autolatch_x_offset, AtlasHistogram::MAX_HIST_SIZE), 0);
                autolatch_peak_bin[pce][spot] = bcehist->hist.maxVal[0];
                autolatch_data_size[pce][spot] = end_bin;
                memset(autolatch_data, 0, start_bin * sizeof(double));
                for(int i = start_bin; i < end_bin; i++)
                {
                    autolatch_data[pce][spot][i] = ((double)hist->bins[i - autolatch_x_offset]) * autolatch_y_scale;
                }
            }
        }

        /* Handle Viewer Modes */
        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(stream_radio)))
        {
            if((plot_buf_max_size < 0) || (plot_buf.length() < plot_buf_max_size))
            {
                unsigned char* view_hist_mem = new unsigned char[size];
                memcpy(view_hist_mem, hist, size);
                view_hist_t* view_hist = (view_hist_t*)view_hist_mem;
                plot_buf.add(view_hist);
            }
        }
        else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(buffer_radio)))
        {
            if((plot_buf_max_size < 0) || (plot_buf.length() < plot_buf_max_size))
            {
                unsigned char* view_hist_mem = new unsigned char[size];
                memcpy(view_hist_mem, hist, size);
                view_hist_t* view_hist = (view_hist_t*)view_hist_mem;
                plot_buf.add(view_hist);
            }
            else
            {
                // wait on condition being signaled, meanwhile release gtkmut
                gdk_threads_leave();
                pthread_mutex_lock(&bufmut);
                pthread_cond_wait(&bufcond, &bufmut);
                gdk_threads_enter();
                // all histogram data has been cleared and deleted, can now continue to add this histogram
                unsigned char* view_hist_mem = new unsigned char[size];
                memcpy(view_hist_mem, hist, size);
                view_hist_t* view_hist = (view_hist_t*)view_hist_mem;
                plot_buf.add(view_hist);
            }
        }
        else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(sample_radio)))
        {
            if( (hist->pceNum < NUM_PCES && hist->type < AtlasHistogram::NUM_TYPES) &&
                (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pktfilter[hist->pceNum][hist->type]))) &&
                (plot_empty_hists == true || hist->sum > 0) )
            {
                unsigned char* view_hist_mem = new unsigned char[size];
                memcpy(view_hist_mem, hist, size);
                view_hist_t* view_hist = (view_hist_t*)view_hist_mem;
                if(plot_buf.length() == 0)
                {
                    plot_buf.add(view_hist);
                }
                else
                {
                    plot_buf.set(0, view_hist);
                }
                pthread_cond_signal(&drawcond);
            }
        }

        if(plot_buf.length() == 1)  // plot first histogram
        {
            pthread_cond_signal(&drawcond);
        }

        setLabel(numsel_label, "%d", plot_buf.length());
    }
    gdk_threads_leave();
}

/*--------------------------------------------------------------------------------------
  -- chstatHandler  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
void Viewer::chstatHandler(chStat_t* chstat, int size)
{
    (void)size;

    /* Get PCE Index */
    int pce = chstat->pce;
    if(pce >= 0 && pce < NUM_PCES)
    {
        /* Build channel stats display */
        static char info[5000];
        sprintf(info, "        STATCNT   NUMTAGS   NUMDUPR   TDCCALR   MINCALR   MAXCALR   AVGCALR   NUMDUPF   TDCCALF   MINCALF   MAXCALF   AVGCALF   BIAS      DEADTIME\n");
        for(int i = 0; i < NUM_CHANNELS; i++)
        {
            char chinfo[512];
            sprintf(chinfo, "[%-2d] %10d%10d%10d%10.3lf%10.3lf%10.3lf%10.3lf%10d%10.3lf%10.3lf%10.3lf%10.3lf%10.3lf%10.3lf\n",
                            i+1, chstat->statcnt, chstat->rx_cnt[i],
                            chstat->num_dupr[i], chstat->tdc_calr[i], chstat->min_calr[i], chstat->max_calr[i], chstat->avg_calr[i],
                            chstat->num_dupf[i], chstat->tdc_calf[i], chstat->min_calf[i], chstat->max_calf[i], chstat->avg_calf[i],
                            chstat->bias[i], chstat->dead_time[i]);
            strcat(info, chinfo);
        }

        /* Draw display to text buf */
        gdk_threads_enter();
        {
            gtk_text_buffer_set_text (GTK_TEXT_BUFFER(chstat_textbuf_info[pce]), info, -1);
        }
        gdk_threads_leave();
    }
    else
    {
        mlog(CRITICAL, "invalid pce number provided in channel statistics: %d ...exiting thread!\n", pce);
    }
}

/*--------------------------------------------------------------------------------------
  -- txstatHandler  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
void Viewer::txstatHandler(txStat_t* txstat, int size)
{
    (void)size;

    gdk_threads_enter();
    {
        int pce = txstat->pce;
        if(pce >= 0 && pce < NUM_PCES)
        {
            setLabel(txstat_label_statcnt[pce],  "%d",    txstat->statcnt);
            setLabel(txstat_label_txcnt[pce],    "%d",    txstat->txcnt);
            setLabel(txstat_label_mindelta[pce], "%.1lf", txstat->min_delta);
            setLabel(txstat_label_maxdelta[pce], "%.1lf", txstat->max_delta);
            setLabel(txstat_label_avgdelta[pce], "%.1lf", txstat->avg_delta);

            double delta_s = 0.0;
            if(txstat->std_tags[STRONG_SPOT] != 0.0) delta_s = MAX(txstat->avg_tags[STRONG_SPOT] - txstat->min_tags[STRONG_SPOT], txstat->max_tags[STRONG_SPOT] - txstat->avg_tags[STRONG_SPOT]) / txstat->std_tags[STRONG_SPOT];
            double delta_w = 0.0;
            if(txstat->std_tags[WEAK_SPOT] != 0.0)  delta_w = MAX(txstat->avg_tags[WEAK_SPOT] - txstat->min_tags[WEAK_SPOT], txstat->max_tags[WEAK_SPOT] - txstat->avg_tags[WEAK_SPOT]) / txstat->std_tags[WEAK_SPOT];
            double delta   = MAX(delta_s, delta_w);

            char taginfo[1024];
            sprintf(taginfo, "       ST    WK\nMin%6d%6d\nMax%6d%6d\nAvg%6.1lf%6.1lf\nStd%6.1lf%6.1lf\n\nOutlier: %.1lf (sigma)\n",
                    txstat->min_tags[STRONG_SPOT], txstat->min_tags[WEAK_SPOT],
                    txstat->max_tags[STRONG_SPOT], txstat->max_tags[WEAK_SPOT],
                    txstat->avg_tags[STRONG_SPOT], txstat->avg_tags[WEAK_SPOT],
                    txstat->std_tags[STRONG_SPOT], txstat->std_tags[WEAK_SPOT],
                    delta);

            gtk_text_buffer_set_text (GTK_TEXT_BUFFER(txstat_textbuf_taginfo[pce]),  taginfo, -1);
        }
        else
        {
            mlog(CRITICAL, "invalid pce number provided in transmit statistics: %d ...exiting thread!\n", pce);
        }
    }
    gdk_threads_leave();
}

/*--------------------------------------------------------------------------------------
  -- reportHandler  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
void Viewer::reportHandler(reportStat_t* reportstat, int size)
{
    (void)size;

    char siginfo[2048];
    sprintf(siginfo, "%6s%24s%24s%24s\n%12s%12s%12s%12s%12s%12s%12s\n%12s%12.1lf%12.1lf%12.1lf%12.1lf%12.1lf%12.1lf\n%12s%12.6lf%12.6lf%12.6lf%12.6lf%12.6lf%12.6lf\n%12s%12.3lf%12.3lf%12.3lf%12.3lf%12.3lf%12.3lf\n%12s%12.1lf%12.1lf%12.1lf%12.1lf%12.1lf%12.1lf\n%12s%12.1lf%12.1lf%12.1lf%12.1lf%12.1lf%12.1lf\n%12s%12.3lf%12.3lf%12.3lf%12.3lf%12.3lf%12.3lf\n%12s%12.1lf%12.1lf%12.1lf%12.1lf%12.1lf%12.1lf\n%12s%12.4e%12.4e%12.4e%12.4e%12.4e%12.4e\n",
            "", "PCE 1", "PCE 2", "PCE 3",
            "", "STRONG", "WEAK", "STRONG", "WEAK", "STRONG", "WEAK",
            "TOF(ns):",       reportstat->spot[0].sigrng,  reportstat->spot[1].sigrng,  reportstat->spot[2].sigrng,  reportstat->spot[3].sigrng,  reportstat->spot[4].sigrng,  reportstat->spot[5].sigrng,
            "Bckgnd(MHz):",   reportstat->spot[0].bkgnd,   reportstat->spot[1].bkgnd,   reportstat->spot[2].bkgnd,   reportstat->spot[3].bkgnd,   reportstat->spot[4].bkgnd,   reportstat->spot[5].bkgnd,
            "Rx(pe):",        reportstat->spot[0].sigpes,  reportstat->spot[1].sigpes,  reportstat->spot[2].sigpes,  reportstat->spot[3].sigpes,  reportstat->spot[4].sigpes,  reportstat->spot[5].sigpes,
            "RWS(ns):",       reportstat->spot[0].rws,     reportstat->spot[1].rws,     reportstat->spot[2].rws,     reportstat->spot[3].rws,     reportstat->spot[4].rws,     reportstat->spot[5].rws,
            "RWW(ns):",       reportstat->spot[0].rww,     reportstat->spot[1].rww,     reportstat->spot[2].rww,     reportstat->spot[3].rww,     reportstat->spot[4].rww,     reportstat->spot[5].rww,
            "TEP(pe):",       reportstat->spot[0].teppe,   reportstat->spot[1].teppe,   reportstat->spot[2].teppe,   reportstat->spot[3].teppe,   reportstat->spot[4].teppe,   reportstat->spot[5].teppe,
            "ATTEN:",         reportstat->spot[0].bceatten,reportstat->spot[1].bceatten,reportstat->spot[2].bceatten,reportstat->spot[3].bceatten,reportstat->spot[4].bceatten,reportstat->spot[5].bceatten,
            "POWER(W):",      reportstat->spot[0].bcepower,reportstat->spot[1].bcepower,reportstat->spot[2].bcepower,reportstat->spot[3].bcepower,reportstat->spot[4].bcepower,reportstat->spot[5].bcepower);

    gdk_threads_enter();
    {
        gtk_text_buffer_set_text (GTK_TEXT_BUFFER(analysis_textbuf),  siginfo, -1);
    }
    gdk_threads_leave();
}

/*--------------------------------------------------------------------------------------
  -- timestatHandler  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
void Viewer::timestatHandler(timeStat_t* timestat, int size)
{
    (void)size;

    gdk_threads_enter();
    {
        const char* sc_1pps_source_str = "INVALID";
        if(timestat->sc_1pps_source == SC_1PPS_A) sc_1pps_source_str = "SC_1PPS_A";
        else if(timestat->sc_1pps_source == SC_1PPS_B) sc_1pps_source_str = "SC_1PPS_B";

        const char* uso_source_str = "INVALID";
        if(timestat->uso_source == USO_A) uso_source_str = "USO_A";
        else if(timestat->uso_source == USO_B) uso_source_str = "USO_B";

        const char* gps_sync_source_str = "INVALID";
        if(timestat->gps_sync_source == GPS_TIME) gps_sync_source_str = "GPS_TIME";
        else if(timestat->gps_sync_source == SC_TIME) gps_sync_source_str = "SC_TIME";

        const char* int_1pps_source_str = "INVALID";
        if(timestat->int_1pps_source == DISABLED_1PPS_SRC) int_1pps_source_str = "DISABLED_1PPS_SRC";
        else if(timestat->int_1pps_source == SC_1PPS_A_SRC) int_1pps_source_str = "SC_1PPS_A_SRC";
        else if(timestat->int_1pps_source == SC_1PPS_B_SRC) int_1pps_source_str = "SC_1PPS_B_SRC";
        else if(timestat->int_1pps_source == ASC_1PPS_SRC) int_1pps_source_str = "ASC_1PPS_SRC";
        else if(timestat->int_1pps_source == UNK_1PPS_SRC) int_1pps_source_str = "UNK_1PPS_SRC";

        char timeinfo[2048];
        sprintf(timeinfo,   "%20s%20.9lf\n%20s%20.9lf\n%20s%20.9lf\n%20s%20.9lf\n%20s%20.9lf, %.9lf, %.9lf\n%20s%20.1lf ns per sec\n%20s%20ld\n%20s%20s\n%20s%20s\n%20s%20s\n%20s%20s\n%20s%20s\n%20s%20u\n",
                            "ASC 1PPS GPS:",        timestat->asc_1pps_time,
                            "SC 1PPS FREQ:",        timestat->sc_1pps_freq,
                            "ASC 1PPS FREQ:",       timestat->asc_1pps_freq,
                            "TQ FREQ:",             timestat->tq_freq,
                            "MF FREQ:",             timestat->mf_freq[0], timestat->mf_freq[1], timestat->mf_freq[2],
                            "USO Drift:",           (1.0 - timestat->uso_freq),
                            "AMET Delta:",          timestat->sc_to_asc_1pps_amet_delta,
                            "SC 1PPS SOURCE:",      sc_1pps_source_str,
                            "USO SOURCE:",          uso_source_str,
                            "GPS SYNC SOURCE:",     gps_sync_source_str,
                            "INT 1PPS SOURCE:",     int_1pps_source_str,
                            "CALC USO FREQ:",       timestat->uso_freq_calc ? "YES" : "NO",
                            "ERROR COUNT:",         timestat->errorcnt);

        gtk_text_buffer_set_text (GTK_TEXT_BUFFER(time_textbuf),  timeinfo, -1);
    }
    gdk_threads_leave();
}

/*--------------------------------------------------------------------------------------
  -- quitCmd  -
  -------------------------------------------------------------------------------------*/
int Viewer::quitCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argv;
    (void)argc;

    appQuit(window, NULL);

    return 0;
}

/*--------------------------------------------------------------------------------------
  -- setParsersCmd  -
  -------------------------------------------------------------------------------------*/
int Viewer::setParsersCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    if(argc != NUM_PROTOCOLS)
    {
        mlog(CRITICAL, "Invalid number of parsers supplied, expecting %d\n", NUM_PROTOCOLS);
        return -1;
    }
    
    /* Get Parsers */
    for(int i = 0; i < NUM_PROTOCOLS; i++)
    {
        parser_qlist[i] = StringLib::duplicate(argv[i]);
        mlog(INFO, "Setting queue %s to protocol %s\n", argv[i], PROTOCOL_LIST[i]);
    }

    return 0;
}

/*--------------------------------------------------------------------------------------
  -- setPlayRateCmd  -
  -------------------------------------------------------------------------------------*/
int Viewer::setPlayRateCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    double hz = strtod(argv[0], NULL);

    if(hz <= 0.0 || hz > 50.0)
    {
        mlog(ERROR, "attempting to set play rate out of bounds (0.0, 50]: %lf\n", hz);
        return -1;
    }

    play_hz = hz;

    return 0;
}

/*--------------------------------------------------------------------------------------
  -- setDataModeCmd  -
  -------------------------------------------------------------------------------------*/
int Viewer::setDataModeCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    Viewer::data_mode_t mode;
    if( (strcmp(argv[0], "STREAM") == 0) || (strcmp(argv[0], "stream") == 0) )
    {
        mode = Viewer::STREAM_MODE;
    }
    else if( (strcmp(argv[0], "BUFFER") == 0) || (strcmp(argv[0], "buffer") == 0) )
    {
        mode = Viewer::BUFFER_MODE;
    }
    else if( (strcmp(argv[0], "SAMPLE") == 0) || (strcmp(argv[0], "sample") == 0) )
    {
        mode = Viewer::SAMPLE_MODE;
    }
    else
    {
        return -1;
    }

    setDataMode(mode);

    return 0;
}

/*--------------------------------------------------------------------------------------
  -- clearPlotsCmd  -
  -------------------------------------------------------------------------------------*/
int Viewer::clearPlotsCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    clearPlots();

    return 0;
}

/*--------------------------------------------------------------------------------------
  -- setPlotBufSizeCmd  -
  -------------------------------------------------------------------------------------*/
int Viewer::setPlotBufSizeCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    setPlotBufSize((int)strtol(argv[0], NULL, 0));

    return 0;
}

/*--------------------------------------------------------------------------------------
  -- setPlotEmptyCmd  -
  -------------------------------------------------------------------------------------*/
int Viewer::setPlotEmptyCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    bool enable;
    if(!StringLib::str2bool(argv[0], &enable)) return -1;

    setPlotEmpty(enable);

    return 0;
}

/*--------------------------------------------------------------------------------------
  -- overrideBinsizeCmd  -
  -------------------------------------------------------------------------------------*/
int Viewer::overrideBinsizeCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    double binsize = strtod(argv[0], NULL);
    if(binsize <= 0.0 || binsize > 500.0)
    {
        mlog(ERROR, "attempting to set binsize to nonsensical value: %lf", binsize);
        return -1;
    }

    overridePlotBinsize(binsize);

    return 0;
}

/*--------------------------------------------------------------------------------------
  -- usePlotBinsizeCmd  -
  -------------------------------------------------------------------------------------*/
int Viewer::usePlotBinsizeCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argv;
    (void)argc;

    usePlotBinsize();

    return 0;
}

/*--------------------------------------------------------------------------------------
  -- setPlotFftCmd  -
  -------------------------------------------------------------------------------------*/
int Viewer::setPlotFftCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    bool enable;
    if(!StringLib::str2bool(argv[0], &enable)) return -1;

    setPlotFFT(enable);

    return 0;
}

/*--------------------------------------------------------------------------------------
  -- setAutoWaveLatchCmd  -
  -------------------------------------------------------------------------------------*/
int Viewer::setAutoWaveLatchCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    bool enable;
    if(!StringLib::str2bool(argv[0], &enable)) return -1;

    if(enable)
    {
        if(argc < 3) return -1;

        int subtype = strtol(argv[1], NULL, 0);

        bool autoalign = false;
        int alignment = 0;
        if(strcmp(argv[2], "AUTO") == 0 || strcmp(argv[2], "auto") == 1)
        {
            autoalign = true;
        }
        else
        {
            alignment = strtol(argv[2], NULL, 0);
        }

        double scale = 1.0;
        if(argc == 4)
        {
            scale = strtod(argv[3], NULL);
        }

        setAutoWaveLatch(enable, autoalign, alignment, scale, subtype);
    }
    else
    {
        setAutoWaveLatch(enable, false, 0, 1.0, 0);
    }

    return 0;
}

/*--------------------------------------------------------------------------------------
  -- attachHstvsCmdQCmd  -
  -------------------------------------------------------------------------------------*/
int Viewer::attachHstvsCmdQCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    const char* name = StringLib::checkNullStr(argv[0]);
    const char* q_name = StringLib::checkNullStr(argv[1]);

    if(hstvs_name != NULL) delete hstvs_name;

    if(name != NULL)
    {
        hstvs_name = StringLib::duplicate(name);
    }
    else
    {
        hstvs_name = NULL;
    }

    if(q_name != NULL)
    {
        hstvsq = new Publisher(q_name);
    }
    else
    {
        hstvsq = NULL;
    }

    return 0;
}

/*--------------------------------------------------------------------------------------
  -- displayUtcCmd  -
  -------------------------------------------------------------------------------------*/
int Viewer::displayUtcCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    bool enable;
    if(!StringLib::str2bool(argv[0], &enable)) return -1;

    display_utc = enable;

    return 0;
}

/******************************************************************************
 STATIC PRIVATE FUNCTIONS
 ******************************************************************************/

/*--------------------------------------------------------------------------------------
  -- Data Thread  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
void* Viewer::dataThread(void* parm)
{
    Viewer* viewer = (Viewer*)parm;

    while(true)
    {
        Subscriber::msgRef_t ref;
        int status = viewer->recdataq->receiveRef(ref, SYS_TIMEOUT);
        if(status == MsgQ::STATE_TIMEOUT)
        {
            if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(viewer->sample_radio)))
            {
                /* Autodetect Histogrammer Q Overflow and Auto Clear */
                if(viewer->recdataq->getCount() == viewer->recdataq->getDepth())
                {
                    viewer->clearPlots();
                    viewer->autoflush_cnt++;
                }
            }
        }
        else if(status == MsgQ::STATE_OKAY)
        {
            try
            {
                RecordInterface recif((unsigned char*)ref.data, ref.size);
                if(recif.isRecordType(BceHistogram::rec_type))                  viewer->histHandler((view_hist_t*)recif.getRecordData(), recif.getRecordDataSize());
                else if(recif.isRecordType(TimeTagHistogram::rec_type[0]))      viewer->histHandler((view_hist_t*)recif.getRecordData(), recif.getRecordDataSize());
                else if(recif.isRecordType(TimeTagHistogram::rec_type[1]))      viewer->histHandler((view_hist_t*)recif.getRecordData(), recif.getRecordDataSize());
                else if(recif.isRecordType(TimeTagHistogram::rec_type[2]))      viewer->histHandler((view_hist_t*)recif.getRecordData(), recif.getRecordDataSize());
                else if(recif.isRecordType(AltimetryHistogram::rec_type[0]))    viewer->histHandler((view_hist_t*)recif.getRecordData(), recif.getRecordDataSize());
                else if(recif.isRecordType(AltimetryHistogram::rec_type[1]))    viewer->histHandler((view_hist_t*)recif.getRecordData(), recif.getRecordDataSize());
                else if(recif.isRecordType(AltimetryHistogram::rec_type[2]))    viewer->histHandler((view_hist_t*)recif.getRecordData(), recif.getRecordDataSize());
                else if(recif.isRecordType(TxStat::rec_type))                   viewer->txstatHandler((txStat_t*)recif.getRecordData(), recif.getRecordDataSize());
                else if(recif.isRecordType(ChStat::rec_type))                   viewer->chstatHandler((chStat_t*)recif.getRecordData(), recif.getRecordDataSize());
                else if(recif.isRecordType(TimeStat::rec_type))                 viewer->timestatHandler((timeStat_t*)recif.getRecordData(), recif.getRecordDataSize());
                else if(recif.isRecordType(ReportProcessorStatistic::rec_type)) viewer->reportHandler((reportStat_t*)recif.getRecordData(), recif.getRecordDataSize());
                else if(!recif.isRecordType(SigStat::rec_type)) // this is the only one ignored
                {
                    mlog(ERROR, "Unhandled record received by viewer: %s\n", recif.getRecordType());
                }
            }
            catch (const InvalidRecordException& e)
            {
                mlog(CRITICAL, "Failed to parse serial data <%s> of size %d!\n", (unsigned char*)ref.data, ref.size);
                mlog(CRITICAL, "ERROR: %s\n", e.what());
            }

            viewer->recdataq->dereference(ref);
        }
        else
        {
            mlog(CRITICAL, "Failed to read data queue, status: %d\n", status);
            sleep(1);
        }

    }

    mlog(CRITICAL, "Exiting viewer data thread!\n");
    return NULL;
}

/*--------------------------------------------------------------------------------------
  -- Plot Thread  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
void* Viewer::plotThread(void* parm)
{
    Viewer* viewer = (Viewer*)parm;
    if(viewer == NULL) return NULL;

    double data[AtlasHistogram::MAX_HIST_SIZE];
    memset(data, 0, sizeof(data));

    sigStat_t sigstats[NUM_PCES];
    memset(sigstats, 0, sizeof(sigstats));

    while(true)
    {
        /* Wait for Draw Signal */
        pthread_mutex_lock(&viewer->drawmut);
        pthread_cond_wait(&viewer->drawcond, &viewer->drawmut);

        /* Populate Plot Buffer */
        gdk_threads_enter();
        {
            int    numpoints = 1;
            double maxvalue  = 1.0;
            double binsize   = 0.0;
            double xrange[2] = {0.0, 1.0};
            double yrange[2] = {0.0, 1.0};
            double fftdata[AtlasHistogram::MAX_HIST_SIZE];
            view_hist_t* core = NULL;
            try { core = viewer->plot_buf[viewer->plot_buf_index]; }
            catch(std::out_of_range& e) { (void)e; }

            /* Load Histogram */
            if( (core != NULL) &&
                (viewer->plot_buf.length() != 0) &&
                (viewer->plot_buf_index < viewer->plot_buf.length()) )
            {
                numpoints = core->size;
                maxvalue  = (double)core->maxVal[0] * 1.05;
                binsize   = core->binSize;

                if(numpoints > AtlasHistogram::MAX_HIST_SIZE)
                {
                    mlog(CRITICAL, "attempting to plot histogram which is too large, %d\n", numpoints);
                    numpoints = AtlasHistogram::MAX_HIST_SIZE;
                }

                /* Perform Frequency Transform on Data */
                if(viewer->plot_fft == true)
                {
                    numpoints &= ~1;
                    maxvalue = MathLib::FFT(fftdata, core->bins, numpoints);

                    if(viewer->clear_accum == true)
                    {
                        viewer->clear_accum = false;
                        memset(data, 0, sizeof(data));
                    }

                    for(int i = 0; i < numpoints; i++)
                    {
                        if(viewer->plot_accum == false)
                        {
                            data[i] = fftdata[i];
                        }
                        else
                        {
                            data[i] += fftdata[i];
                        }
                    }

                }
                /* Override Bin Size */
                else if(viewer->plot_override_binsize == true)
                {
                    double  binratio    = core->binSize / viewer->plot_binsize;
                    double  j           = 0.0;

                    numpoints *= binratio;
                    binsize = viewer->plot_binsize;

                    mlog(DEBUG, "Scaling histogram to %d using ratio %.2lf\n", numpoints, binratio);
                    if(numpoints >= AtlasHistogram::MAX_HIST_SIZE)
                    {
                        mlog(INFO, "Truncating histogram to fit within memory constraints %d -> %d\n", numpoints, AtlasHistogram::MAX_HIST_SIZE);
                        numpoints = AtlasHistogram::MAX_HIST_SIZE;
                    }

                    if(viewer->plot_accum == false || viewer->clear_accum == true)
                    {
                        viewer->clear_accum = false;
                        memset(data, 0, sizeof(data));
                    }

                    if(binratio < 1.0)
                    {
                        for(int i = 0; i < core->size; i++)
                        {
                            data[(int)j] += core->bins[i];
                            if(data[(int)j] > maxvalue) maxvalue = data[(int)j];
                            j += binratio;
                        }
                    }
                    else
                    {
                        for(int i = 0; i < numpoints; i++)
                        {
                            data[i] += core->bins[(int)j] / binratio;
                            if(data[i] > maxvalue) maxvalue = data[i];
                            j += 1.0 / binratio;
                        }
                    }
                }
                /* Translate Data */
                else
                {
                    if(viewer->clear_accum == true)
                    {
                        viewer->clear_accum = false;
                        memset(data, 0, sizeof(data));
                    }

                    for(int i = 0; i < numpoints; i++)
                    {
                        if(viewer->plot_accum == false)
                        {
                            data[i] = (double)core->bins[i];
                        }
                        else
                        {
                            data[i] += (double)core->bins[i];
                        }
                    }
                }

                /* Handle Auto-Latching */
                if(viewer->autolatch_active)
                {
                    /* Grabe PCE and Spot */
                    int pce = core->pceNum;
                    int spot = (core->type == AtlasHistogram::STT || core->type == AtlasHistogram::SAL || core->type == AtlasHistogram::SAM) ? STRONG_SPOT : WEAK_SPOT;

                    /* Calculate Bin Shift */
                    int bin_shift = 0;
                    if(viewer->autolatch_auto_peak_align)
                    {
                        bin_shift = core->maxVal[0] - viewer->autolatch_peak_bin[pce][spot];
                        if(bin_shift >= AtlasHistogram::MAX_HIST_SIZE)
                        {
                            mlog(WARNING, "Unable to shift auto-latched histogram: %d\n", bin_shift);
                            bin_shift = 0;
                        }
                    }

                    /* Calcualte New Histogram Parameters */
                    int copy_offset = abs(bin_shift);
                    int copy_size = MAX(viewer->autolatch_data_size[pce][spot] - copy_offset, 0);
                    viewer->latched_data_size = viewer->autolatch_data_size[pce][spot];

                    /* Shift Auto-Latched Histogram into Latched Data */
                    if(bin_shift < 0)
                    {
                        memcpy(viewer->latched_data, &viewer->autolatch_data[pce][spot][copy_offset], copy_size * sizeof(double));
                        memset(&viewer->latched_data[copy_size], 0, copy_offset * sizeof(double));
                    }
                    else
                    {
                        memset(viewer->latched_data, 0, copy_offset * sizeof(double));
                        memcpy(&viewer->latched_data[copy_offset], viewer->autolatch_data[pce][spot], copy_size * sizeof(double));
                    }
                }

                /* Set Default Ranges */
                xrange[1] = (double)numpoints;
                yrange[1] = maxvalue;
                if(viewer->plot_accum) yrange[1] *= ++(viewer->num_accum);

                /* Populate UTC Time */
                char timeinfo[32];
                if(viewer->display_utc)
                {
                    struct tm utctime;
                    time_t utc = core->gpsAtMajorFrame + 315964800;
                    gmtime_r((time_t*)&utc, &utctime);
                    size_t numchars = strftime(timeinfo, 32, "%y:%j:%H:%M:%S", &utctime);
                    sprintf(&timeinfo[numchars], ":%.3lf", core->gpsAtMajorFrame - (long)core->gpsAtMajorFrame);
                }
                else
                {
                    sprintf(timeinfo, "%lf", core->gpsAtMajorFrame);
                }

                /* Populate Display */
                viewer->setLabel(viewer->plot_label_type,        "%s",       AtlasHistogram::type2str(core->type));
                viewer->setLabel(viewer->plot_label_pce,         "%d",       core->pceNum + 1);
                viewer->setLabel(viewer->plot_label_binsize,     "%.2lf",    binsize * 20.0 / 3.0);
                viewer->setLabel(viewer->plot_label_histsize,    "%d",       numpoints);
                viewer->setLabel(viewer->plot_label_mfpavail,    "%s",       core->majorFramePresent ? "yes" : "no");
                viewer->setLabel(viewer->plot_label_mfc,         "%ld",      core->majorFrameCounter);
                viewer->setLabel(viewer->plot_label_utc,         "%s",       timeinfo);
                viewer->setLabel(viewer->plot_label_rws,         "%.1lf",    core->rangeWindowStart / 10.0);
                viewer->setLabel(viewer->plot_label_rww,         "%.1lf",    core->rangeWindowWidth / 10.0);
                viewer->setLabel(viewer->plot_label_numtx,       "%d",       core->transmitCount);
                viewer->setLabel(viewer->plot_label_intperiod,   "%d",       core->integrationPeriod * 200);
                viewer->setLabel(viewer->plot_label_mbps,        "%.1lf",    core->integrationPeriod ? core->pktBytes * ((8 * (50 / core->integrationPeriod)) / 1000000.0) : 0.0);

                char signalinfo[512];
                sprintf(signalinfo, "%-18s%-10.1lf\n%-18s%-10.6lf\n%-18s%-10.3lf\n%-18s%-10.3lf (%d, %d)\n%-18s%-10d\n%-18s%-10.1lf",
                        "TOF(ns):", core->signalRange,
                        "Backgnd(MHz):", core->noiseFloor,
                        "Return(per shot):", core->signalEnergy,
                        "TEP(pe):", core->tepEnergy, core->ignoreStartBin, core->ignoreStopBin,
                        "SigBin:", core->beginSigBin,
                        "SigWid(ns):", core->signalWidth);

                char metainfo[512];
                sprintf(metainfo, "         Bin   Val\nMax[1]: %4d %5d\nMax[2]: %4d %5d\nMax[3]: %4d %5d\n\nTotal Count: %5d",
                        core->maxBin[0], core->maxVal[0],
                        core->maxBin[1], core->maxVal[1],
                        core->maxBin[2], core->maxVal[2],
                        core->sum);

                char channels[256];
                if(core->type == AtlasHistogram::STT || core->type == AtlasHistogram::WTT)
                {
                    TimeTagHistogram::ttHist_t* ttHist = (TimeTagHistogram::ttHist_t*)core;
                    sprintf(channels, "STRONG %4d %4d %4d %4d\n       %4d %4d %4d %4d\n       %4d %4d %4d %4d\n       %4d %4d %4d %4d\n\nWEAK   %4d %4d %4d %4d\n",
                            ttHist->channelCounts[0], ttHist->channelCounts[1], ttHist->channelCounts[2], ttHist->channelCounts[3],
                            ttHist->channelCounts[4], ttHist->channelCounts[5], ttHist->channelCounts[6], ttHist->channelCounts[7],
                            ttHist->channelCounts[8], ttHist->channelCounts[9], ttHist->channelCounts[10],ttHist->channelCounts[11],
                            ttHist->channelCounts[12],ttHist->channelCounts[13],ttHist->channelCounts[14],ttHist->channelCounts[15],
                            ttHist->channelCounts[16],ttHist->channelCounts[17],ttHist->channelCounts[18],ttHist->channelCounts[19]);
                }
                else
                {
                    channels[0] = '\0';
                }

                char ancillary[512];
                ancillary[0] = '\0';
                if(core->majorFramePresent)
                {
                    if(core->majorFrameData.EDACStatusBits)              strcat(ancillary, "EDAC\n");
                    if(core->majorFrameData.RangeWindowDropout_Err)      strcat(ancillary, "Range Window Dropout\n");
                    if(core->majorFrameData.TDC_StrongPath_Err)          strcat(ancillary, "TDC Strong Path\n");
                    if(core->majorFrameData.TDC_WeakPath_Err)            strcat(ancillary, "TDC Weak Path\n");
                    if(core->majorFrameData.DidNotFinishTransfer_Err)    strcat(ancillary, "Did Not Finish Transfer\n");
                    if(core->majorFrameData.SDRAMMismatch_Err)           strcat(ancillary, "SDRAM Mismatch\n");
                    if(core->majorFrameData.DidNotFinishWritingData_Err) strcat(ancillary, "Did Not Finish Writing Data\n");
                    if(core->majorFrameData.CardDataNotFinished_Err)     strcat(ancillary, "Card Data Not Finished\n");
                    if(core->majorFrameData.TDC_FIFOWentFull)            strcat(ancillary, "TDC\n");
                    if(core->majorFrameData.EventTag_FIFOWentFull)       strcat(ancillary, "Event Tag\n");
                    if(core->majorFrameData.Burst_FIFOWentFull)          strcat(ancillary, "Burst\n");
                    if(core->majorFrameData.StartTag_FIFOWentFull)       strcat(ancillary, "Start Tag\n");
                    if(core->majorFrameData.Tracking_FIFOWentFull)       strcat(ancillary, "Tracking\n");
                    if(core->majorFrameData.PacketizerA_FIFOWentFull)    strcat(ancillary, "Packetizer A\n");
                    if(core->majorFrameData.PacketizerB_FIFOWentFull)    strcat(ancillary, "Packetizer B\n");
                }

                if(core->type == AtlasHistogram::GRL)
                {
                    BceHistogram::bceHist_t* bceHist = (BceHistogram::bceHist_t*)core;
                    char bce_ancillary[256];
                    snprintf(bce_ancillary, 256, "%10s%10d\n%10s%10d\n%10s%10d\n%10s%10d\n",
                                "TYPE     ", bceHist->subtype,
                                "GRL:     ", bceHist->grl,
                                "OSC ID:  ", bceHist->oscId,
                                "OSC CHAN:", bceHist->oscCh);
                    strcat(ancillary, bce_ancillary);
                }

                char dlbinfo[512];
                if(core->type == AtlasHistogram::STT || core->type == AtlasHistogram::WTT)
                {
                    TimeTagHistogram::ttHist_t* ttHist = (TimeTagHistogram::ttHist_t*)core;
                    sprintf(dlbinfo, "   Mask   Start   Width   Events\n%2d %05X  %-7d %-7d %-7d\n%2d %05X  %-7d %-7d %-7d\n%2d %05X  %-7d %-7d %-7d\n%2d %05X  %-7d %-7d %-7d\n",
                            0, ttHist->downlinkBands[0].mask, ttHist->downlinkBands[0].start, ttHist->downlinkBands[0].width, ttHist->downlinkBandsTagCnt[0],
                            1, ttHist->downlinkBands[1].mask, ttHist->downlinkBands[1].start, ttHist->downlinkBands[1].width, ttHist->downlinkBandsTagCnt[1],
                            2, ttHist->downlinkBands[2].mask, ttHist->downlinkBands[2].start, ttHist->downlinkBands[2].width, ttHist->downlinkBandsTagCnt[2],
                            3, ttHist->downlinkBands[3].mask, ttHist->downlinkBands[3].start, ttHist->downlinkBands[3].width, ttHist->downlinkBandsTagCnt[3]);
                }
                else
                {
                    dlbinfo[0] = '\0';
                }

                char statinfo[2048];
                if(core->type == AtlasHistogram::STT || core->type == AtlasHistogram::WTT)
                {
                    TimeTagHistogram::ttHist_t* ttHist = (TimeTagHistogram::ttHist_t*)core;
                    sprintf(statinfo, "NUMTAGS: %d\nNUMSEGS: %-9d\nMFC: %-9dHDR: %-9d\nFMT: %-9dDLB: %-9d\nTAG: %-9dPKT: %-9d",
                                    ttHist->pktStats.sum_tags,    ttHist->pktStats.segcnt,      ttHist->pktStats.mfc_errors,
                                    ttHist->pktStats.hdr_errors,  ttHist->pktStats.fmt_errors,  ttHist->pktStats.dlb_errors,  ttHist->pktStats.tag_errors,  ttHist->pktStats.pkt_errors);
                }
                else
                {
                    statinfo[0] = '\0';
                }


                gtk_text_buffer_set_text (GTK_TEXT_BUFFER(viewer->plot_textbuf_signal),     signalinfo, -1);
                gtk_text_buffer_set_text (GTK_TEXT_BUFFER(viewer->plot_textbuf_meta),       metainfo,   -1);
                gtk_text_buffer_set_text (GTK_TEXT_BUFFER(viewer->plot_textbuf_channels),   channels,   -1);
                gtk_text_buffer_set_text (GTK_TEXT_BUFFER(viewer->plot_textbuf_ancillary),  ancillary,  -1);
                gtk_text_buffer_set_text (GTK_TEXT_BUFFER(viewer->plot_textbuf_dlbs),       dlbinfo,    -1);
                gtk_text_buffer_set_text (GTK_TEXT_BUFFER(viewer->plot_textbuf_stats),      statinfo,   -1);

                /* Populate Current Display on Analysis Tab */
                if((core->pceNum >= 0 && core->pceNum < NUM_PCES) &&
                   (core->type == AtlasHistogram::STT || core->type == AtlasHistogram::WTT))
                {
                    sigStat_t* sigptr = &sigstats[core->pceNum];

                    int spot;
                    if(core->type == AtlasHistogram::STT)   spot = STRONG_SPOT;
                    else                                    spot = WEAK_SPOT;

                    sigptr->sigrng[spot]    = core->signalRange;
                    sigptr->bkgnd[spot]     = core->noiseFloor;
                    sigptr->sigpes[spot]    = core->signalEnergy;
                    sigptr->rws[spot]       = core->rangeWindowStart;
                    sigptr->rww[spot]       = core->rangeWindowWidth;
                    sigptr->teppe[spot]     = core->tepEnergy;

                    char siginfo[2048];
                    sprintf(siginfo, "%6s%24s%24s%24s\n%12s%12s%12s%12s%12s%12s%12s\n%12s%12.1lf%12.1lf%12.1lf%12.1lf%12.1lf%12.1lf\n%12s%12.6lf%12.6lf%12.6lf%12.6lf%12.6lf%12.6lf\n%12s%12.3lf%12.3lf%12.3lf%12.3lf%12.3lf%12.3lf\n%12s%12.1lf%12.1lf%12.1lf%12.1lf%12.1lf%12.1lf\n%12s%12.1lf%12.1lf%12.1lf%12.1lf%12.1lf%12.1lf\n%12s%12.3lf%12.3lf%12.3lf%12.3lf%12.3lf%12.3lf\n",
                                "", "PCE 1", "PCE 2", "PCE 3",
                                "", "STRONG", "WEAK", "STRONG", "WEAK", "STRONG", "WEAK",
                                "TOF(ns):",       sigstats[0].sigrng[STRONG_SPOT],  sigstats[0].sigrng[WEAK_SPOT],  sigstats[1].sigrng[STRONG_SPOT],  sigstats[1].sigrng[WEAK_SPOT],  sigstats[2].sigrng[STRONG_SPOT],  sigstats[2].sigrng[WEAK_SPOT],
                                "Bckgnd(MHz):",   sigstats[0].bkgnd[STRONG_SPOT],   sigstats[0].bkgnd[WEAK_SPOT],   sigstats[1].bkgnd[STRONG_SPOT],   sigstats[1].bkgnd[WEAK_SPOT],   sigstats[2].bkgnd[STRONG_SPOT],   sigstats[2].bkgnd[WEAK_SPOT],
                                "Rx(pe):",        sigstats[0].sigpes[STRONG_SPOT],  sigstats[0].sigpes[WEAK_SPOT],  sigstats[1].sigpes[STRONG_SPOT],  sigstats[1].sigpes[WEAK_SPOT],  sigstats[2].sigpes[STRONG_SPOT],  sigstats[2].sigpes[WEAK_SPOT],
                                "RWS(ns):",       sigstats[0].rws[STRONG_SPOT],     sigstats[0].rws[WEAK_SPOT],     sigstats[1].rws[STRONG_SPOT],     sigstats[1].rws[WEAK_SPOT],     sigstats[2].rws[STRONG_SPOT],     sigstats[2].rws[WEAK_SPOT],
                                "RWW(ns):",       sigstats[0].rww[STRONG_SPOT],     sigstats[0].rww[WEAK_SPOT],     sigstats[1].rww[STRONG_SPOT],     sigstats[1].rww[WEAK_SPOT],     sigstats[2].rww[STRONG_SPOT],     sigstats[2].rww[WEAK_SPOT],
                                "TEP(pe):",       sigstats[0].teppe[STRONG_SPOT],   sigstats[0].teppe[WEAK_SPOT],   sigstats[1].teppe[STRONG_SPOT],   sigstats[1].teppe[WEAK_SPOT],   sigstats[2].teppe[STRONG_SPOT],   sigstats[2].teppe[WEAK_SPOT]);

                    gtk_text_buffer_set_text (GTK_TEXT_BUFFER(viewer->current_textbuf), siginfo, -1);
                }
            }
            else
            {
                viewer->setLabel(viewer->plot_label_type,        "nill");
                viewer->setLabel(viewer->plot_label_pce,         "nill");
                viewer->setLabel(viewer->plot_label_binsize,     "nill");
                viewer->setLabel(viewer->plot_label_histsize,    "nill");
                viewer->setLabel(viewer->plot_label_mfpavail,    "nill");
                viewer->setLabel(viewer->plot_label_mfc,         "nill");
                viewer->setLabel(viewer->plot_label_utc,         "nill");
                viewer->setLabel(viewer->plot_label_rws,         "nill");
                viewer->setLabel(viewer->plot_label_rww,         "nill");
                viewer->setLabel(viewer->plot_label_numtx,       "nill");
                viewer->setLabel(viewer->plot_label_intperiod,   "nill");
                viewer->setLabel(viewer->plot_label_mbps,        "nill");

                gtk_text_buffer_set_text (GTK_TEXT_BUFFER(viewer->plot_textbuf_signal),     "nill", -1);
                gtk_text_buffer_set_text (GTK_TEXT_BUFFER(viewer->plot_textbuf_meta),       "nill", -1);
                gtk_text_buffer_set_text (GTK_TEXT_BUFFER(viewer->plot_textbuf_channels),   "nill", -1);
                gtk_text_buffer_set_text (GTK_TEXT_BUFFER(viewer->plot_textbuf_ancillary),  "nill", -1);
            }

            /* Plot Histogram */
            gtk_adjustment_set_value(GTK_ADJUSTMENT(viewer->selector_adj), (double)viewer->plot_buf_index);
            gtk_adjustment_set_upper(GTK_ADJUSTMENT(viewer->selector_adj), (double)viewer->plot_buf.length());
            gtk_adjustment_set_step_increment(GTK_ADJUSTMENT(viewer->selector_adj), 1.0);
            gtk_widget_queue_draw(GTK_WIDGET(viewer->selector_slider));

            if(viewer->plot_action == PLOT_NORMAL)
            {
                if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(viewer->fixx2spinner_check)))
                {
                    xrange[1] = gtk_adjustment_get_value(GTK_ADJUSTMENT(viewer->scalex_adj));
                }
                else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(viewer->fixx2rww_check)))
                {
                    if(core != NULL)
                    {
                        xrange[1] = core->rangeWindowWidth * 3.0 / 20.0 / core->binSize;
                    }
                }

                if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(viewer->fixy2spinner_check)))
                {
                    yrange[1] = gtk_adjustment_get_value(GTK_ADJUSTMENT(viewer->scaley_adj));
                }
            }
            else
            {
                xrange[0] = MAX(-10.0, viewer->plot_x_range[0]);
                xrange[1] = MIN(xrange[1], viewer->plot_x_range[1]);
                yrange[0] = MAX(-10.0, viewer->plot_y_range[0]);
                yrange[1] = MIN(yrange[1], viewer->plot_y_range[1]);
            }

            viewer->plot_x_range[0] = xrange[0];
            viewer->plot_x_range[1] = xrange[1];
            viewer->plot_y_range[0] = yrange[0];
            viewer->plot_y_range[1] = yrange[1];

            gtk_plot_set_range(GTK_PLOT(viewer->hist_plot), xrange[0], xrange[1], yrange[0], yrange[1]);
            gtk_plot_set_ticks(GTK_PLOT(viewer->hist_plot), GTK_PLOT_AXIS_X, round((xrange[1] - xrange[0]) * 0.1) + 1, 1);
            gtk_plot_set_ticks(GTK_PLOT(viewer->hist_plot), GTK_PLOT_AXIS_Y, round((yrange[1] - yrange[0]) * 0.1) + 1, 1);
            gtk_plot_data_set_numpoints(viewer->plot_dataset, numpoints);
            gtk_plot_data_set_y(viewer->plot_dataset, data);
            gtk_plot_data_set_x(viewer->plot_dataset, viewer->plot_x_vals);

            if(viewer->latch_active)
            {
                gtk_plot_set_range(GTK_PLOT(viewer->latch_plot), xrange[0], xrange[1], yrange[0], yrange[1]);
                gtk_plot_set_ticks(GTK_PLOT(viewer->latch_plot), GTK_PLOT_AXIS_X, round((xrange[1] - xrange[0]) * 0.1) + 1, 1);
                gtk_plot_set_ticks(GTK_PLOT(viewer->latch_plot), GTK_PLOT_AXIS_Y, round((yrange[1] - yrange[0]) * 0.1) + 1, 1);
                gtk_plot_data_set_numpoints(viewer->latch_dataset, viewer->latched_data_size);
                gtk_plot_data_set_y(viewer->latch_dataset, viewer->latched_data);
                gtk_plot_data_set_x(viewer->latch_dataset, viewer->plot_x_vals);
            }

            gtk_plot_canvas_paint(GTK_PLOT_CANVAS(viewer->plot_canvas));
            gtk_widget_queue_draw(GTK_WIDGET(viewer->plot_canvas));
        }
        gdk_threads_leave();
    }

    mlog(CRITICAL, "Exiting viewer plot thread!\n");
    return NULL;
}

/*--------------------------------------------------------------------------------------
  -- Applcation Stats Thread  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
void* Viewer::appstatThread(void* parm)
{
    Viewer* viewer = (Viewer*)parm;

    if(viewer == NULL) return NULL;

    MsgQ* scidataq = NULL;

    while(true)
    {
        sleep(1); // nominal 1Hz update rate

        gdk_threads_enter();
        {
            if(scidataq == NULL)
            {
                if(MsgQ::existQ(viewer->scidataq_name))
                {
                    scidataq = new MsgQ(viewer->scidataq_name);
                }
            }

            unsigned long ccsds_auto_flush_cnt = 0;
            viewer->cmdProc->getCurrentValue(viewer->ccsdsproc_name, CcsdsPacketProcessor::autoFlushCntKey, &ccsds_auto_flush_cnt, sizeof(ccsds_auto_flush_cnt));

            bool sig_change = false;
            bool tep_change = false;
            bool loop_change = false;
            double val = 0.0;
            for(int p = 0; p < NUM_PCES; p++)
            {
                viewer->cmdProc->getCurrentValue(viewer->ttproc_name[p], TimeTagProcessorModule::signalWidthKey , &val, sizeof(val));
                if(val != TimeTagProcessorModule::DEFAULT_SIGNAL_WIDTH)
                {
                    sig_change = true;
                }

                viewer->cmdProc->getCurrentValue(viewer->ttproc_name[p], TimeTagProcessorModule::tepLocationKey , &val, sizeof(val));
                if(val != TimeTagProcessorModule::DEFAULT_TEP_LOCATION)
                {
                    tep_change = true;
                }

                viewer->cmdProc->getCurrentValue(viewer->ttproc_name[p], TimeTagProcessorModule::tepWidthKey , &val, sizeof(val));
                if(val != TimeTagProcessorModule::DEFAULT_TEP_WIDTH)
                {
                    tep_change = true;
                }

                viewer->cmdProc->getCurrentValue(viewer->ttproc_name[p], TimeTagProcessorModule::loopbackLocationKey , &val, sizeof(val));
                if(val != TimeTagProcessorModule::DEFAULT_LOOPBACK_LOCATION)
                {
                    loop_change = true;
                }

                viewer->cmdProc->getCurrentValue(viewer->ttproc_name[p], TimeTagProcessorModule::loopbackWidthKey , &val, sizeof(val));
                if(val != TimeTagProcessorModule::DEFAULT_LOOPBACK_WIDTH)
                {
                    loop_change = true;
                }
            }
            const char* modeinfo = "Normal";
            if(loop_change) modeinfo = "Design";
            else if(sig_change || tep_change) modeinfo = "Cloud";

            char pktinfo[32];
            if(scidataq != NULL)            sprintf(pktinfo, "%d", scidataq->getCount());
            else                            sprintf(pktinfo, "null");

            char histinfo[32];
            if(viewer->recdataq != NULL)    sprintf(histinfo, "%d", viewer->recdataq->getCount());
            else                            sprintf(histinfo, "null");

            int64_t latency = 0;
            viewer->cmdProc->getCurrentValue(viewer->ccsdsproc_name, CcsdsPacketProcessor::latencyKey, &latency, sizeof(latency));

            char* sockinfo = DeviceObject::getDeviceList();

            char msginfo[64];
            int warning_cnt     = LogLib::getLvlCnts(WARNING);
            int error_cnt       = LogLib::getLvlCnts(ERROR);
            int critical_cnt    = LogLib::getLvlCnts(CRITICAL);
            sprintf(msginfo, "%dw, %de, %dc", warning_cnt, error_cnt, critical_cnt);

            char info[1024];
            sprintf(info, "%-8s%14s:%ld\n%-8s%14s:%d\n%-8s%14ld\n%-8s%14s\n%-8s%14s\n\n%s\n%s",
                    "Pkt Q:",   pktinfo, ccsds_auto_flush_cnt,
                    "Rec Q:",   histinfo, viewer->autoflush_cnt,
                    "Latency:", latency,
                    "Version:", BINID,
                    "Mode:",    modeinfo,
                    sockinfo,
                    msginfo);

            gtk_text_buffer_set_text (GTK_TEXT_BUFFER(viewer->app_textbuf_status), info, -1);

            delete [] sockinfo; // free socket info
        }
        gdk_threads_leave();
    }
}

/*--------------------------------------------------------------------------------------
  -- playTimer  -
  --
  --   Notes: Not thread safe!!! must be called with thread protection
  -------------------------------------------------------------------------------------*/
gboolean Viewer::playTimer(gpointer data)
{
    Viewer* viewer = (Viewer*)data;

    if(viewer->nextHist())
    {
        return TRUE;
    }
    else
    {
        viewer->play_active = false;
        return FALSE;
    }
}

/*--------------------------------------------------------------------------------------
  -- deleteEvent  -
  -
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
gboolean Viewer::deleteEvent(GtkWidget* widget, GdkEvent* event, gpointer data)
{
    (void)widget;
    (void)event;
    (void)data;

    return FALSE; /* returning false destroys window as destroy event is issued */
}

/*--------------------------------------------------------------------------------------
  -- appQuit  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::appQuit(GtkWidget* widget, gpointer data)
{
    (void)widget;
    (void)data;
    
    console_quick_exit(0);
}

/*--------------------------------------------------------------------------------------
  -- refreshHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::refreshHandler(GtkButton* button, gpointer user_data)
{
    (void)button;

    Viewer* viewer = (Viewer*)user_data;
    viewer->clearPlots();
    viewer->autoflush_cnt = 0;
}

/*--------------------------------------------------------------------------------------
  -- restoreHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::restoreHandler(GtkButton* button, gpointer user_data)
{
    (void)button;

    Viewer* viewer = (Viewer*)user_data;

    viewer->plot_zoom_level = 0;
    viewer->plot_action = PLOT_NORMAL;

    pthread_cond_signal(&viewer->drawcond);
}

/*--------------------------------------------------------------------------------------
  -- leftArrowHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::leftArrowHandler(GtkButton* button, gpointer user_data)
{
    (void)button;

    Viewer* viewer = (Viewer*)user_data;
    viewer->prevHist();
}

/*--------------------------------------------------------------------------------------
  -- rightArrowHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::rightArrowHandler(GtkButton* button, gpointer user_data)
{
    (void)button;

    Viewer* viewer = (Viewer*)user_data;
    viewer->nextHist();
}

/*--------------------------------------------------------------------------------------
  -- modeHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::modeHandler(GtkButton* button, gpointer user_data)
{
    (void)button;

    Viewer* viewer = (Viewer*)user_data;
    viewer->clearPlots();
}

/*--------------------------------------------------------------------------------------
  -- fixXHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::fixXHandler(GtkButton* button, gpointer user_data)
{
    Viewer* viewer = (Viewer*)user_data;

    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
    {
        if((long)button == (long)viewer->fixx2spinner_check)
        {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(viewer->fixx2rww_check), false);
        }
        else if((long)button == (long)viewer->fixx2rww_check)
        {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(viewer->fixx2spinner_check), false);
        }
    }

    pthread_cond_signal(&viewer->drawcond);
}

/*--------------------------------------------------------------------------------------
  -- fixYHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::fixYHandler(GtkButton* button, gpointer user_data)
{
    (void)button;

    Viewer* viewer = (Viewer*)user_data;
    pthread_cond_signal(&viewer->drawcond);
}

/*--------------------------------------------------------------------------------------
  -- plotFFTHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::plotFFTHandler(GtkButton* button, gpointer user_data)
{
    Viewer* viewer = (Viewer*)user_data;

    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
    {
        viewer->plot_fft = true;
    }
    else
    {
        viewer->plot_fft = false;
    }

    pthread_cond_signal(&viewer->drawcond);
}

/*--------------------------------------------------------------------------------------
  -- selectorHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::selectorHandler(GtkAdjustment *adjustment, gpointer user_data)
{
    Viewer* viewer = (Viewer*)user_data;

    int plot_index = (int)round(gtk_adjustment_get_value(adjustment));
    if(plot_index >= 0 && plot_index < viewer->plot_buf.length())
    {
        viewer->plot_buf_index = plot_index;
    }

    pthread_cond_signal(&viewer->drawcond);
}

/*--------------------------------------------------------------------------------------
  -- playHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::playHandler(GtkButton* button, gpointer user_data)
{
    (void)button;

    Viewer* viewer = (Viewer*)user_data;

    if(viewer->play_active) return;

    viewer->play_active = true;
    viewer->play_id = gtk_timeout_add((guint32)(round(1000.0 / viewer->play_hz) + 1), viewer->playTimer, viewer);
}

/*--------------------------------------------------------------------------------------
  -- stopHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::stopHandler(GtkButton* button, gpointer user_data)
{
    (void)button;

    Viewer* viewer = (Viewer*)user_data;

    if(!viewer->play_active) return;

    viewer->play_active = false;
    gtk_timeout_remove(viewer->play_id);
}


/*--------------------------------------------------------------------------------------
  -- fileOpenHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::fileOpenHandler(GtkButton* button, gpointer user_data)
{
    (void)button;

    Viewer* viewer = (Viewer*)user_data;

    GtkWidget* dialog = gtk_file_chooser_dialog_new ("Open File", GTK_WINDOW(viewer->window), GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER (dialog), TRUE);

    GtkWidget* protocol_combo = gtk_combo_box_new_text();
    for(int i = 0; i < NUM_PROTOCOLS; i++)
    {
        gtk_combo_box_append_text(GTK_COMBO_BOX(protocol_combo), PROTOCOL_LIST[i]);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(protocol_combo), 0);

    gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER (dialog), protocol_combo);

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
        int protocol_index = gtk_combo_box_get_active (GTK_COMBO_BOX(protocol_combo));
        if(protocol_index < NUM_PROTOCOLS && protocol_index >= 0)
        {
            const char* format = FORMAT_LIST[protocol_index];

            /* Configure for Processing Files */
            viewer->clearPlots();
            viewer->setPlotBufSize(-1);

            /* Get Filenames */
            GSList* filename_list = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER (dialog));

            /* Get Number of Files in List */
            int numfiles = 0;
            GSList* cur1 = filename_list;
            while(cur1 != NULL) { cur1 = cur1->next; numfiles++; }

            /* Populate Filename String */
            SafeString filename_string;
            GSList* cur2 = filename_list;
            for(int i = 0; i < numfiles; i++)
            {
                filename_string += (const char*)cur2->data;
                filename_string += " ";
                mlog(CRITICAL, "File %s added for opening\n", (char*)cur2->data);
                cur2 = cur2->next;
            }

            /* Check File Reader Active */
            if(viewer->cmdProc->getObject(VIEWER_FILE_READER, DeviceIO::OBJECT_TYPE) != NULL)
            {
                if(viewer->file_reader != NULL)
                {
                    viewer->cmdProc->deleteObject(viewer->file_reader->getName());
                    viewer->file_reader = NULL;
                }
                else
                {
                    mlog(CRITICAL, "Unable to reach file reader %s to stop it!\n", VIEWER_FILE_READER);
                }
            }

            /* Create File Reader */
           
            viewer->cmdProc->postCommand("NEW DEVICE_READER %s FILE %s %s %s", VIEWER_FILE_READER, format, filename_string.getString(), viewer->parser_qlist[protocol_index]);
            LocalLib::sleep(1); // time for above command to execute            
            viewer->file_reader = (DeviceReader*)viewer->cmdProc->getObject(VIEWER_FILE_READER, DeviceIO::OBJECT_TYPE);
            if(viewer->file_reader == NULL)
            {
                mlog(CRITICAL, "Unable to register file reader for viewer: %s\n", VIEWER_FILE_READER);
            }

            /* Free Resources */
            GSList* cur3 = filename_list;
            while(cur3 != NULL) { g_free(cur3->data); cur3 = cur3->next; }
            g_slist_free(filename_list);
        }
        else
        {
            mlog(CRITICAL, "invalid protocol index selected: %d\n", protocol_index);
        }
    }

    gtk_widget_destroy (dialog);
}

/*--------------------------------------------------------------------------------------
  -- fileExportHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::fileExportHandler(GtkButton* button, gpointer user_data)
{
    (void)button;

    Viewer* viewer = (Viewer*)user_data;

    GtkWidget* dialog = gtk_file_chooser_dialog_new ("Save File", GTK_WINDOW(viewer->window), GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);

    GtkWidget* protocol_combo = gtk_combo_box_new_text();
    gtk_combo_box_append_text(GTK_COMBO_BOX(protocol_combo), "Plot (PostScript)"); // 0
    gtk_combo_box_append_text(GTK_COMBO_BOX(protocol_combo), "Bin Values (Text)"); // 1
    gtk_combo_box_append_text(GTK_COMBO_BOX(protocol_combo), "Statistics Report (Text)"); // 2
    gtk_combo_box_set_active(GTK_COMBO_BOX(protocol_combo), 0);
    gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER (dialog), protocol_combo);

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
        int protocol_index = gtk_combo_box_get_active (GTK_COMBO_BOX(protocol_combo));
        char* filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

        if(protocol_index == 0)
        {
            if(gtk_plot_canvas_export_ps(GTK_PLOT_CANVAS(viewer->plot_canvas), filename, GTK_PLOT_LANDSCAPE, FALSE, GTK_PLOT_LEGAL) == FALSE)
            {
                mlog(ERROR, "Unable to export histogram image to file: %s\n", filename);
            }
            else
            {
                mlog(INFO, "Exported histogram image to file: %s\n", filename);
            }
        }
        else if(protocol_index == 1)
        {
            FILE* fp = fopen(filename, "w");
            if(fp != NULL)
            {
                view_hist_t* core = viewer->plot_buf[viewer->plot_buf_index];
                if( (core != NULL) &&
                    (viewer->plot_buf.length() != 0) &&
                    (viewer->plot_buf_index < viewer->plot_buf.length()) )
                {
                    for(int i = 0; i < core->size; i++)
                    {
                        fprintf(fp, "%d, %d\n", i, core->bins[i]);
                    }
                }
                fclose(fp);

                mlog(INFO, "Wrote histogram file: %s\n", filename);
            }
            else
            {
                mlog(ERROR, "Unable to open file to export histogram\n");
            }
        }
        else if(protocol_index == 2)
        {
            viewer->cmdProc->postCommand("%s::GENERATE_REPORT %s", viewer->reportproc_name, filename);
        }

        g_free (filename);
    }
    gtk_widget_destroy (dialog);
}

/*--------------------------------------------------------------------------------------
  -- pceFilterHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::pceFilterHandler(GtkButton* button, gpointer user_data)
{
    Viewer* viewer = (Viewer*)user_data;

    for(int p = 0; p < NUM_PCES; p++)
    {
        if((void*)viewer->pcefilter[p] == (void*)button)
        {
            bool setting = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
            for(int i = 0; i < AtlasHistogram::NUM_TYPES; i++)
            {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(viewer->pktfilter[p][i]), setting);
            }
            break;
        }
    }

    pthread_cond_signal(&viewer->drawcond);
}

/*--------------------------------------------------------------------------------------
  -- plotResizeHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
gboolean Viewer::plotResizeHandler(GtkWidget* widget, GtkAllocation* allocation, gpointer user_data)
{
    (void)widget;

    Viewer* viewer = (Viewer*)user_data;
    viewer->plot_width = allocation->width;
    viewer->plot_height = allocation->height;
    gtk_plot_canvas_set_size(GTK_PLOT_CANVAS(viewer->plot_canvas), viewer->plot_width, viewer->plot_height);
    pthread_cond_signal(&viewer->drawcond);

    return TRUE;
}

/*--------------------------------------------------------------------------------------
  -- plotResizeHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
#define LEFT_MOUSE_BUTTON   1
#define RIGHT_MOUSE_BUTTON  3
#define ZOOM_SCALAR         0.5
#define AXIS_OFFSET         0.07
gboolean Viewer::plotMouseHandler(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
    (void)widget;

    Viewer* viewer = (Viewer*)user_data;

    static int    pressed = 0;
    static double press_x = 0;
    static double press_y = 0;

    double x_axis_offset = viewer->plot_width * AXIS_OFFSET;
    double y_axis_offset = viewer->plot_height * AXIS_OFFSET;

    double axis_width = viewer->plot_width * (1.0 - (AXIS_OFFSET * 2));
    double axis_height = viewer->plot_height * (1.0 - (AXIS_OFFSET * 2));

    double x = event->button.x;
    double y = event->button.y;

    double x_size = viewer->plot_x_range[1] - viewer->plot_x_range[0];
    double y_size = viewer->plot_y_range[1] - viewer->plot_y_range[0];

    /* ZOOMING */
    if(event->type == GDK_2BUTTON_PRESS)
    {
        viewer->plot_action = PLOT_INTERACTIVE;
        double zoom_factor = ZOOM_SCALAR;

        if(event->button.button == LEFT_MOUSE_BUTTON)       /* ZOOM IN */
        {
            zoom_factor += 0.0;
            viewer->plot_zoom_level += 1;
        }
        else if(event->button.button == RIGHT_MOUSE_BUTTON) /* ZOOM OUT */
        {
            zoom_factor += 1.0;
            viewer->plot_zoom_level -= 1;
        }

        double norm_x  = MAX(x - x_axis_offset, 0.0) / axis_width;
        double x_trans = (norm_x * x_size) + viewer->plot_x_range[0];
        double x_scale = (zoom_factor * x_size) * 0.5;

        viewer->plot_x_range[0] = x_trans - x_scale;
        viewer->plot_x_range[1] = x_trans + x_scale;

        double norm_y  = MAX((axis_height + y_axis_offset) - y, 0.0) / axis_height;
        double y_trans = (norm_y * y_size) + viewer->plot_y_range[0];
        double y_scale = (zoom_factor * y_size) * 0.5;

        viewer->plot_y_range[0] = y_trans - y_scale;
        viewer->plot_y_range[1] = y_trans + y_scale;

        pthread_cond_signal(&viewer->drawcond);

    }
    else if(event->type == GDK_BUTTON_PRESS)
    {
        pressed = event->button.button;
        press_x = x;
        press_y = y;
    }
    else if(event->type == GDK_BUTTON_RELEASE)
    {
        pressed = 0;

        double delta_x = (press_x - x) / axis_width;
        double delta_y = (y - press_y) / axis_height;

        if(fabs(delta_x) > 0.01 || (fabs(delta_y) > 0.01))
        {
            /* PANNING */
            if(event->button.button == RIGHT_MOUSE_BUTTON)
            {
                viewer->plot_action = PLOT_INTERACTIVE;

                viewer->plot_x_range[0] += delta_x * x_size;
                viewer->plot_x_range[1] += delta_x * x_size;

                viewer->plot_y_range[0] += delta_y * y_size;
                viewer->plot_y_range[1] += delta_y * y_size;

                pthread_cond_signal(&viewer->drawcond);
            }
            /* ZOOM BOX */
            else if(event->button.button == LEFT_MOUSE_BUTTON)
            {
                viewer->plot_action = PLOT_INTERACTIVE;

                double x1       = MIN(x, press_x);
                double x2       = MAX(x, press_x);
                double norm_x1  = MAX(x1 - x_axis_offset, 0.0) / axis_width;
                double norm_x2  = MAX(x2 - x_axis_offset, 0.0) / axis_width;

                viewer->plot_x_range[0] = viewer->plot_x_range[0] + norm_x1 * x_size;
                viewer->plot_x_range[1] = viewer->plot_x_range[0] + norm_x2 * x_size;

                double y1       = MAX(y, press_y);
                double y2       = MIN(y, press_y);
                double norm_y1  = MAX((axis_height + y_axis_offset) - y1, 0.0) / axis_height;
                double norm_y2  = MAX((axis_height + y_axis_offset) - y2, 0.0) / axis_height;

                viewer->plot_y_range[0] = viewer->plot_y_range[0] + norm_y1 * y_size;
                viewer->plot_y_range[1] = viewer->plot_y_range[0] + norm_y2 * y_size;

                pthread_cond_signal(&viewer->drawcond);
            }
        }
    }
    else if(event->type == GDK_MOTION_NOTIFY)
    {
        if(pressed == LEFT_MOUSE_BUTTON)
        {
//            GdkGC* gc = gdk_gc_new(widget->window);
//            gdk_gc_set_function(gc, GDK_XOR);

            static int prev_width = 0, prev_height = 0, prev_origin_x = 0, prev_origin_y = 0;

            gdk_draw_rectangle(widget->window, widget->style->white_gc, FALSE, prev_origin_x, prev_origin_y, prev_width, prev_height);
            int width       = abs(x - press_x);
            int height      = abs(y - press_y);
            int origin_x    = MIN(press_x, x);
            int origin_y    = MIN(press_y, y);
            gdk_draw_rectangle(widget->window, widget->style->black_gc, FALSE, origin_x, origin_y, width, height);
            prev_width      = width;
            prev_height     = height;
            prev_origin_x   = origin_x;
            prev_origin_y   = origin_y;
        }
    }

    return TRUE;
}

/*--------------------------------------------------------------------------------------
  -- txstatClearHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::txstatClearHandler(GtkButton* button, gpointer user_data)
{
    Viewer* viewer = (Viewer*)user_data;

    for(int p = 0; p < NUM_PCES; p++)
    {
        if((long)button == (long)viewer->txstat_button_clear[p])
        {
            viewer->cmdProc->postCommand("%s.txStat::CLEAR ONCE", viewer->ttproc_name[p]);

            viewer->setLabel(viewer->txstat_label_statcnt[p],  "nill");
            viewer->setLabel(viewer->txstat_label_txcnt[p],    "nill");
            viewer->setLabel(viewer->txstat_label_mindelta[p], "nill");
            viewer->setLabel(viewer->txstat_label_maxdelta[p], "nill");
            viewer->setLabel(viewer->txstat_label_avgdelta[p], "nill");
            gtk_text_buffer_set_text (GTK_TEXT_BUFFER(viewer->txstat_textbuf_taginfo[p]), "", -1);

            break;
        }
    }
}

/*--------------------------------------------------------------------------------------
  -- chstatClearHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::chstatClearHandler(GtkButton* button, gpointer user_data)
{
    Viewer* viewer = (Viewer*)user_data;

    for(int p = 0; p < NUM_PCES; p++)
    {
        for(int i = 0; i < NUM_CHANNELS; i++)
        {
            if((long)button == (long)viewer->chstat_button_clear[p][i])
            {
                viewer->cmdProc->postCommand("%s::CLEAR_CH_STAT ONCE %d %d", viewer->ttproc_name[p], p, i);
                break;
            }
        }
    }
}

/*--------------------------------------------------------------------------------------
  -- connectionHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::connectionHandler(GtkButton* button, gpointer user_data)
{
    (void)button;
    (void)user_data;

    mlog(CRITICAL, "Option no longer supported\n");
}

/*--------------------------------------------------------------------------------------
  -- latchHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::latchHandler(GtkButton* button, gpointer user_data)
{
    (void)button;

    Viewer* viewer = (Viewer*)user_data;

    if(viewer->latch_active == false)
    {
        viewer->latch_active = true;
        gtk_button_set_label (button, "Unlatch");

        if(viewer->plot_buf.length() > 0)
        {
            try
            {
                if( (viewer->plot_buf_index < viewer->plot_buf.length()) && (viewer->plot_buf[viewer->plot_buf_index] != NULL) )
                {
                    view_hist_t* latched_hist = viewer->plot_buf[viewer->plot_buf_index];
                    viewer->latched_data_size = latched_hist->size;
                    for(int i = 0; i < viewer->latched_data_size; i++)
                    {
                        viewer->latched_data[i] = (double)latched_hist->bins[i];
                    }

                    gtk_widget_show(GTK_WIDGET(viewer->latch_dataset));
                }
                else
                {
                    mlog(CRITICAL, "attempt to latch out of bounds plot index\n");
                }
            }
            catch(std::out_of_range& e)
            {
                mlog(CRITICAL, "exceeded range of plot buffer: %s\n", e.what());
            }
        }
    }
    else
    {
        viewer->latch_active = false;
        gtk_button_set_label (button, "Latch");
        gtk_widget_hide(GTK_WIDGET(viewer->latch_dataset));
    }
}

/*--------------------------------------------------------------------------------------
  -- hstvsHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
#define MAX_HSTVS_CMD_SIZE 256
void Viewer::hstvsHandler(GtkButton* button, gpointer user_data)
{
    (void)button;
    char cmd[MAX_HSTVS_CMD_SIZE];

    Viewer* viewer = (Viewer*)user_data;

    if(viewer->hstvsq != NULL && viewer->hstvs_name != NULL)
    {
        const char* r1 = gtk_entry_buffer_get_text (viewer->hstvs_range_buf[0]);
        const char* p1 = gtk_entry_buffer_get_text (viewer->hstvs_pe_buf[0]);
        const char* w1 = gtk_entry_buffer_get_text (viewer->hstvs_width_buf[0]);
        const char* r2 = gtk_entry_buffer_get_text (viewer->hstvs_range_buf[1]);
        const char* p2 = gtk_entry_buffer_get_text (viewer->hstvs_pe_buf[1]);
        const char* w2 = gtk_entry_buffer_get_text (viewer->hstvs_width_buf[1]);
        const char* r3 = gtk_entry_buffer_get_text (viewer->hstvs_range_buf[2]);
        const char* p3 = gtk_entry_buffer_get_text (viewer->hstvs_pe_buf[2]);
        const char* w3 = gtk_entry_buffer_get_text (viewer->hstvs_width_buf[2]);
        const char* n  = gtk_entry_buffer_get_text (viewer->hstvs_noise_buf);

        snprintf(cmd, MAX_HSTVS_CMD_SIZE, "%s::CLEAR_INPUTS", viewer->hstvs_name);
        viewer->hstvsq->postCopy(cmd, strlen(cmd) + 1);

        mlog(CRITICAL, ">>> %s\n", cmd);
        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(viewer->hstvs_strong_check)))
        {
            snprintf(cmd, MAX_HSTVS_CMD_SIZE, "%s::LOAD 0.0 %s %s %s %s %s %s %s %s %s %s STRONG", viewer->hstvs_name, r1, p1, w1, r2, p2, w2, r3, p3, w3, n);
            viewer->hstvsq->postCopy(cmd, strlen(cmd) + 1);
            mlog(CRITICAL, ">>> %s\n", cmd);
        }

        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(viewer->hstvs_weak_check)))
        {
            snprintf(cmd, MAX_HSTVS_CMD_SIZE, "%s::LOAD 0.0 %s %s %s %s %s %s %s %s %s %s WEAK", viewer->hstvs_name, r1, p1, w1, r2, p2, w2, r3, p3, w3, n);
            viewer->hstvsq->postCopy(cmd, strlen(cmd) + 1);
            mlog(CRITICAL, ">>> %s\n", cmd);
        }

        snprintf(cmd, MAX_HSTVS_CMD_SIZE, "%s::GENERATE_COMMANDS", viewer->hstvs_name);
        viewer->hstvsq->postCopy(cmd, strlen(cmd) + 1);
        mlog(CRITICAL, ">>> %s\n", cmd);
    }
}

/*--------------------------------------------------------------------------------------
  -- accumHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::accumHandler(GtkButton* button, gpointer user_data)
{
    Viewer* viewer = (Viewer*)user_data;

    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
    {
        viewer->plot_accum = true;
    }
    else
    {
        viewer->plot_accum = false;
        viewer->num_accum = 1;
    }
}

/*--------------------------------------------------------------------------------------
  -- clearAccumHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::clearAccumHandler(GtkButton* button, gpointer user_data)
{
    (void)button;
    Viewer* viewer = (Viewer*)user_data;
    viewer->clear_accum = true;
    viewer->num_accum = 1;
    pthread_cond_signal(&viewer->drawcond);
}

/*--------------------------------------------------------------------------------------
  -- intPeriodHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::intPeriodHandler(GtkButton* button, gpointer user_data)
{
    (void)button;

    Viewer* viewer = (Viewer*)user_data;

    int intperiod = gtk_adjustment_get_value(GTK_ADJUSTMENT(viewer->intperiod_adj));
    viewer->cmdProc->postCommand("%s::INTEGRATE %d 0x4E6", viewer->ttproc_name[0], intperiod);
    viewer->cmdProc->postCommand("%s::INTEGRATE %d 0x4F0", viewer->ttproc_name[1], intperiod);
    viewer->cmdProc->postCommand("%s::INTEGRATE %d 0x4FA", viewer->ttproc_name[2], intperiod);
}

/*--------------------------------------------------------------------------------------
  -- zoomInHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::zoomInHandler(GtkButton* button, gpointer user_data)
{
    (void)button;

    Viewer* viewer = (Viewer*)user_data;
    
    try
    {
        view_hist_t* core = viewer->plot_buf[viewer->plot_buf_index];
        for(int i = 0; i < NUM_PCES; i++)
        {
            viewer->cmdProc->postCommand("%s::SET_TT_ZOOM_OFFSET %.0lf", viewer->ttproc_name[i], core->signalRange - 40.0);
            viewer->cmdProc->postCommand("%s::SET_TT_BINSIZE 0.15", viewer->ttproc_name[i]);
        }
    }
    catch(std::out_of_range& e)
    {
        mlog(CRITICAL, "Attempting to zoom in on non-existent plot!\n");
    }
}

/*--------------------------------------------------------------------------------------
  -- zoomOutHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::zoomOutHandler(GtkButton* button, gpointer user_data)
{
    (void)button;

    Viewer* viewer = (Viewer*)user_data;
    for(int i = 0; i < NUM_PCES; i++)
    {
        viewer->cmdProc->postCommand("%s::SET_TT_BINSIZE REVERT", viewer->ttproc_name[i]);
    }
}

/*--------------------------------------------------------------------------------------
  -- autolatchHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::autolatchHandler (GtkButton* button, gpointer user_data)
{
    Viewer* viewer = (Viewer*)user_data;

    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
    {
        viewer->autolatch_active = true;
        viewer->latch_active = true;
        gtk_widget_show(GTK_WIDGET(viewer->latch_dataset));
    }
    else
    {
        viewer->autolatch_active = false;
        viewer->latch_active = false;
        gtk_widget_hide(GTK_WIDGET(viewer->latch_dataset));
    }
}

/*--------------------------------------------------------------------------------------
  -- fullColHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::fullColHandler (GtkButton* button, gpointer user_data)
{
    Viewer* viewer = (Viewer*)user_data;

    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
    {
        viewer->cmdProc->postCommand("%s::FULL_COL_MODE ENABLE", viewer->ttproc_name[0]);
        viewer->cmdProc->postCommand("%s::FULL_COL_MODE ENABLE", viewer->ttproc_name[1]);
        viewer->cmdProc->postCommand("%s::FULL_COL_MODE ENABLE", viewer->ttproc_name[2]);
    }
    else
    {
        viewer->cmdProc->postCommand("%s::FULL_COL_MODE DISABLE", viewer->ttproc_name[0]);
        viewer->cmdProc->postCommand("%s::FULL_COL_MODE DISABLE", viewer->ttproc_name[1]);
        viewer->cmdProc->postCommand("%s::FULL_COL_MODE DISABLE", viewer->ttproc_name[2]);
    }
}


/*--------------------------------------------------------------------------------------
  -- reportstatClearHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::reportstatClearHandler (GtkButton* button, gpointer user_data)
{
    (void)button;
    Viewer* viewer = (Viewer*)user_data;
    viewer->cmdProc->postCommand("%s.sigStat::CLEAR ONCE", viewer->ttproc_name[0]);
    viewer->cmdProc->postCommand("%s.sigStat::CLEAR ONCE", viewer->ttproc_name[1]);
    viewer->cmdProc->postCommand("%s.sigStat::CLEAR ONCE", viewer->ttproc_name[2]);
    viewer->cmdProc->postCommand("%s::CLEAR ONCE", viewer->reportproc_name);
}

/*--------------------------------------------------------------------------------------
  -- flushHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::flushHandler (GtkButton* button, gpointer user_data)
{
    (void)button;
    Viewer* viewer = (Viewer*)user_data;
    viewer->cmdProc->postCommand("%s::FLUSH", viewer->ccsdsproc_name);
}

/*--------------------------------------------------------------------------------------
  -- autoSetClkHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::autoSetClkHandler (GtkButton* button, gpointer user_data)
{
    Viewer* viewer = (Viewer*)user_data;

    for(int i = 0; i < NUM_PCES; i++)
    {
        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
        {
            viewer->cmdProc->postCommand("%s::AUTO_SET_RULER_CLK ENABLE", viewer->ttproc_name[i]);
        }
        else
        {
            viewer->cmdProc->postCommand("%s::AUTO_SET_RULER_CLK DISABLE", viewer->ttproc_name[i]);
        }
    }
}

/*--------------------------------------------------------------------------------------
  -- timestatClearHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Viewer::timestatClearHandler (GtkButton* button, gpointer user_data)
{
    (void)button;
    Viewer* viewer = (Viewer*)user_data;
    viewer->cmdProc->postCommand("%s::CLEAR_TIME_STAT ONCE", viewer->timeproc_name);
}

