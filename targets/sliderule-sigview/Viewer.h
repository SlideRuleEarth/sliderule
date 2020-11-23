#ifndef __VIEWER_HPP__
#define __VIEWER_HPP__

#include "rtap/core.h"
#include "rtap/ccsds.h"

#include "atlasdefines.h"

#include "AdasSocketReader.h"
#include "DatasrvSocketReader.h"
#include "AtlasHistogram.h"
#include "AltimetryHistogram.h"
#include "TimeTagHistogram.h"
#include "BceHistogram.h"
#include "TimeTagProcessorModule.h"
#include "AltimetryProcessorModule.h"
#include "MajorFrameProcessorModule.h"
#include "TimeProcessorModule.h"
#include "LaserProcessorModule.h"
#include "ReportProcessorStatistic.h"

#include <gtk/gtk.h>
#include <gtkextra/gtkextra.h>

class Viewer: public CommandableObject
{
    public:
        
        /* -------------------------------------
         * Typedefs
         * ------------------------------------- */

        typedef enum {
            STREAM_MODE,
            BUFFER_MODE,
            SAMPLE_MODE
        } data_mode_t;

        typedef AtlasHistogram::hist_t view_hist_t;

        /* -------------------------------------
         * Constants
         * ------------------------------------- */

        static const char* TYPE;

        /* -------------------------------------
         * Methods
         * ------------------------------------- */
        
        static  CommandableObject* createObject (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

    private:

        /* -------------------------------------
         * Typedefs
         * ------------------------------------- */

        typedef enum {
            PLOT_NORMAL,
            PLOT_INTERACTIVE,
        } plot_action_t;

        /* -------------------------------------
         * Constants
         * ------------------------------------- */

        static const int DEFAULT_PLOT_BUF_MAX_SIZE = 0x800; // 2048
        static const int WINDOW_X_SIZE_INIT = 1000;
        static const int WINDOW_Y_SIZE_INIT = 400;
        static const int REQ_VERT_RESOLUTION = 1200;
        static const int NUM_RX_PER_TX = 3;
        static const int NUM_PROTOCOLS = 9;
        
        static const char* PROTOCOL_LIST[NUM_PROTOCOLS];
        static const char* FORMAT_LIST[NUM_PROTOCOLS];
        static const char* VIEWER_FILE_READER;

        /* -------------------------------------
         * Data
         * ------------------------------------- */

        pthread_mutex_t         bufmut;
        pthread_mutexattr_t     bufmut_attr;
        pthread_cond_t          bufcond;

        pthread_mutex_t         drawmut;
        pthread_mutexattr_t     drawmut_attr;
        pthread_cond_t          drawcond;

        bool                    play_active;
        double                  play_hz;
        guint                   play_id;

        Subscriber*             recdataq;
        int                     autoflush_cnt;

        char*                   scidataq_name;
        char*                   ttproc_name[NUM_PCES];
        char*                   reportproc_name;
        char*                   timeproc_name;
        char*                   ccsdsproc_name;
        
        char*                   parser_qlist[NUM_PROTOCOLS];

        char*                   hstvs_name;
        Publisher*              hstvsq;

        DeviceReader*           file_reader;

        MgList<view_hist_t*>    plot_buf;
        int                     plot_buf_index; 	// keeps track of current histogram
	int                     plot_buf_max_size;

        bool                    latch_active;
        double                  latched_data[AtlasHistogram::MAX_HIST_SIZE];
        int                     latched_data_size;
        bool                    autolatch_active;
        double                  autolatch_data[NUM_PCES][NUM_SPOTS][AtlasHistogram::MAX_HIST_SIZE];
        int                     autolatch_data_size[NUM_PCES][NUM_SPOTS];
        int                     autolatch_wave_subtype;
        bool                    autolatch_auto_peak_align;
        int                     autolatch_peak_bin[NUM_PCES][NUM_SPOTS];
        int                     autolatch_x_offset;
        double                  autolatch_y_scale;

        double                  plot_x_vals[AtlasHistogram::MAX_HIST_SIZE];
        double                  bins_in_hist;

        int                     plot_width;
        int                     plot_height;
        double                  plot_x_range[2];    // min, max
        double                  plot_y_range[2];    // min, max
        plot_action_t           plot_action;
        int                     plot_zoom_level;
        bool                    plot_empty_hists;
        bool                    plot_override_binsize;
        double                  plot_binsize; // used only if binsize is overridden
        bool                    plot_fft;
        bool                    plot_accum;
        bool                    clear_accum;
        int                     num_accum; // number of histograms accumulated

        bool                    display_utc;

        PangoFontDescription*   font_desc;
        GtkWidget*              open_button;
        GtkWidget*              export_button;
        GtkWidget*              connection_button;
        GtkObject*              selector_adj;
        GtkWidget*              selector_slider;
        GtkWidget*              numsel_label;
        GtkWidget*              larrow_button;
        GtkWidget*              stop_button;
        GtkWidget*              play_button;
        GtkWidget*              rarrow_button;
        GtkWidget*              refresh_button;
        GtkWidget*              restore_button;
        GtkWidget*              latch_button;
        GtkWidget*              stream_radio;
        GtkWidget*              buffer_radio;
        GtkWidget*              sample_radio;
        GtkWidget*              fixx2spinner_check;
        GtkWidget*              fixy2spinner_check;
        GtkWidget*              fixx2rww_check;
        GtkObject*              scalex_adj;
        GtkObject*              scaley_adj;
        GtkEntryBuffer*         fixy_buf;
        GtkWidget*              pcefilter[NUM_PCES];
        GtkWidget*              pktfilter[NUM_PCES][AtlasHistogram::NUM_TYPES];
        GtkWidget*              plot_container;
        GtkWidget*              plot_canvas;
        GtkWidget*              hist_plot;
        GtkPlotAxis*            x_axis;
        GtkPlotAxis*            y_axis;
        GtkPlotData*            plot_dataset;
        GtkWidget*              latch_plot;
        GtkPlotData*            latch_dataset;
        GtkWidget*              plot_label_type;
        GtkWidget*              plot_label_pce;
        GtkWidget*              plot_label_binsize;
        GtkWidget*              plot_label_histsize;
        GtkWidget*              plot_label_mfpavail;
        GtkWidget*              plot_label_mfc;
        GtkWidget*              plot_label_utc;
        GtkWidget*              plot_label_rws;
        GtkWidget*              plot_label_rww;
        GtkWidget*              plot_label_numtx;
        GtkWidget*              plot_label_intperiod;
        GtkWidget*              plot_label_mbps;
        GtkTextBuffer*          plot_textbuf_signal;
        GtkTextBuffer*          plot_textbuf_meta;
        GtkTextBuffer*          plot_textbuf_channels;
        GtkTextBuffer*          plot_textbuf_ancillary;
        GtkTextBuffer*          plot_textbuf_dlbs;
        GtkTextBuffer*          plot_textbuf_stats;
        GtkWidget*              txstat_button_clear[NUM_PCES];
        GtkWidget*              txstat_label_statcnt[NUM_PCES];
        GtkWidget*              txstat_label_txcnt[NUM_PCES];
        GtkWidget*              txstat_label_mindelta[NUM_PCES];
        GtkWidget*              txstat_label_maxdelta[NUM_PCES];
        GtkWidget*              txstat_label_avgdelta[NUM_PCES];
        GtkTextBuffer*          txstat_textbuf_taginfo[NUM_PCES];
        GtkWidget*              chstat_button_clear[NUM_PCES][NUM_CHANNELS + 1]; // additional 1 for all
        GtkTextBuffer*          chstat_textbuf_info[NUM_PCES];
        GtkEntryBuffer*         hstvs_range_buf[NUM_RX_PER_TX];
        GtkEntryBuffer*         hstvs_pe_buf[NUM_RX_PER_TX];
        GtkEntryBuffer*         hstvs_width_buf[NUM_RX_PER_TX];
        GtkEntryBuffer*         hstvs_noise_buf;
        GtkWidget*              hstvs_strong_check;
        GtkWidget*              hstvs_weak_check;
        GtkWidget*              hstvs_cmd_button;
        GtkWidget*              plotfft_check;
        GtkWidget*              plotaccum_check;
        GtkWidget*              clearaccum_button;
        GtkWidget*              intperiod_spinner_button;
        GtkObject*              intperiod_adj;
        GtkWidget*              zoom_in_button;
        GtkWidget*              zoom_out_button;
        GtkWidget*              autolatch_check;
        GtkWidget*              fullcol_check;
        GtkTextBuffer*          analysis_textbuf;
        GtkTextBuffer*          current_textbuf;
        GtkWidget*              clearsig_button;
        GtkWidget*              flush_button;
        GtkWidget*              autoset_clk_check;
        GtkWidget*              cleartime_button;
        GtkTextBuffer*          time_textbuf;

        GtkWidget*              app_textview_status;
        GtkTextBuffer*          app_textbuf_status;
        GtkWidget*              window;

        /* -------------------------------------
         * Methods
         * ------------------------------------- */

                        Viewer                  (CommandProcessor* cmd_proc, const char* obj_name, const char* _histq_name, const char* _scidataq_name, const char* _ttproc_name[NUM_PCES], const char* _reportproc_name, const char* _timeproc_name, const char* _ccsdsproc_name);
                        ~Viewer                 (void);

        bool            setDataMode             (data_mode_t mode);
        void            setPlotBufSize          (int _plot_buf_max_size);
        void            setPlotEmpty            (bool _plot_empty_hists);
        void            overridePlotBinsize     (double _binsize);
        void            usePlotBinsize          (void);
        void            setPlotFFT              (bool _enable);
        void            setAutoWaveLatch        (bool _enable, bool _autoalign, int _alignment, double _scale, int _wave_subtype);
        
        GtkWidget*      buildIOPanel            (void);
        GtkWidget*      buildSelectionPanel     (void);
        GtkWidget*      buildControlPanel       (void);
        GtkWidget*      buildAppPanel           (void);
        GtkWidget*      buildSettingsPanel      (void);
        GtkWidget*      buildFilterPanel        (void);
        GtkWidget*      buildPlotTab            (void);
        GtkWidget*      buildTxStatTab          (void);
        GtkWidget*      buildChStatTab          (int pce);
        GtkWidget*      buildPktStatTab         (int histtype);
        GtkWidget*      buildHstvsTab           (void);
        GtkWidget*      buildAnalyzeTab         (void);
        GtkWidget*      buildTimeStatTab        (void);
        GtkWidget*      buildPlotPanel          (void);

        void            clearPlots              (void);
        void            setLabel                (GtkWidget* l, const char* fmt, ...);
        bool            nextHist                (void);
        bool            prevHist                (void);

        void            histHandler             (view_hist_t* hist, int size);
        void            chstatHandler           (chStat_t* chstat, int size);
        void            txstatHandler           (txStat_t* txstat, int size);
        void            reportHandler           (reportStat_t* reportstat, int size);
        void            timestatHandler         (timeStat_t* timestat, int size);

        int             quitCmd                 (int argc, char argv[][MAX_CMD_SIZE]);
        int             setParsersCmd           (int argc, char argv[][MAX_CMD_SIZE]);
        int             setPlayRateCmd          (int argc, char argv[][MAX_CMD_SIZE]);
        int             setDataModeCmd          (int argc, char argv[][MAX_CMD_SIZE]);
        int             clearPlotsCmd           (int argc, char argv[][MAX_CMD_SIZE]);
        int             setPlotBufSizeCmd       (int argc, char argv[][MAX_CMD_SIZE]);
        int             setPlotEmptyCmd         (int argc, char argv[][MAX_CMD_SIZE]);
        int             overrideBinsizeCmd      (int argc, char argv[][MAX_CMD_SIZE]);
        int             usePlotBinsizeCmd       (int argc, char argv[][MAX_CMD_SIZE]);
        int             setPlotFftCmd           (int argc, char argv[][MAX_CMD_SIZE]);
        int             setAutoWaveLatchCmd     (int argc, char argv[][MAX_CMD_SIZE]);
        int             attachHstvsCmdQCmd      (int argc, char argv[][MAX_CMD_SIZE]);
        int             displayUtcCmd           (int argc, char argv[][MAX_CMD_SIZE]);

        static void*    dataThread              (void* parm);
        static void*    plotThread              (void* parm);
        static void*    appstatThread           (void* parm);

        static gboolean playTimer               (gpointer data);

        static gboolean deleteEvent             (GtkWidget* widget, GdkEvent* event, gpointer data);
        static void     appQuit                 (GtkWidget* widget, gpointer data);
        static void     refreshHandler          (GtkButton* button, gpointer user_data);
        static void     restoreHandler          (GtkButton* button, gpointer user_data);
        static void     leftArrowHandler        (GtkButton* button, gpointer user_data);
        static void     rightArrowHandler       (GtkButton* button, gpointer user_data);
        static void     modeHandler             (GtkButton* button, gpointer user_data);
        static void     fixXHandler             (GtkButton* button, gpointer user_data);
        static void     fixYHandler             (GtkButton* button, gpointer user_data);
        static void     plotFFTHandler          (GtkButton* button, gpointer user_data);
        static void     selectorHandler         (GtkAdjustment* adjustment, gpointer user_data);
        static void     playHandler             (GtkButton* button, gpointer user_data);
        static void     stopHandler             (GtkButton* button, gpointer user_data);
        static void     fileOpenHandler         (GtkButton* button, gpointer user_data);
        static void     fileExportHandler       (GtkButton* button, gpointer user_data);
        static void     pceFilterHandler        (GtkButton* button, gpointer user_data);
        static gboolean plotResizeHandler       (GtkWidget* widget, GtkAllocation* allocation, gpointer user_data);
        static gboolean plotMouseHandler        (GtkWidget* widget, GdkEvent* event, gpointer user_data);
        static gboolean plotMotionHandler       (GtkWidget* widget, GdkEvent* event, gpointer user_data);
        static void     txstatClearHandler      (GtkButton* button, gpointer user_data);
        static void     chstatClearHandler      (GtkButton* button, gpointer user_data);
        static void     connectionHandler       (GtkButton* button, gpointer user_data);
        static void     latchHandler            (GtkButton* button, gpointer user_data);
        static void     hstvsHandler            (GtkButton* button, gpointer user_data);
        static void     accumHandler            (GtkButton* button, gpointer user_data);
        static void     clearAccumHandler       (GtkButton* button, gpointer user_data);
        static void     intPeriodHandler        (GtkButton* button, gpointer user_data);
        static void     zoomInHandler           (GtkButton* button, gpointer user_data);
        static void     zoomOutHandler          (GtkButton* button, gpointer user_data);
        static void     autolatchHandler        (GtkButton* button, gpointer user_data);
        static void     fullColHandler          (GtkButton* button, gpointer user_data);
        static void     reportstatClearHandler  (GtkButton* button, gpointer user_data);
        static void     flushHandler            (GtkButton* button, gpointer user_data);
        static void     autoSetClkHandler       (GtkButton* button, gpointer user_data);
        static void     timestatClearHandler    (GtkButton* button, gpointer user_data);
};

#endif  /* __VIEWER_HPP__ */
