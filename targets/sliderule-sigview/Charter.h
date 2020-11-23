#ifndef __CHARTER_HPP__
#define __CHARTER_HPP__

#include <gtk/gtk.h>
#include <gtkextra/gtkextra.h>

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

/***********************
 TYPEDEFS
************************/

typedef enum {
    BLUE_MARKER = 0,
    GREEN_MARKER = 1
} marker_t;

class DataPlot;

/***********************
 CHARTER
************************/

class Charter: public CommandableObject
{
    public:

        /* -------------------------------------
         * Constants
         * ------------------------------------- */

        static const char* TYPE;
        static const int MAX_DATA_POINTS = 10000;

        /* -------------------------------------
         * Methods
         * ------------------------------------- */

        static CommandableObject*   createObject        (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

        static  double              intAvg              (int num, double curr_avg, double new_val);
        static  void                setLabel            (GtkWidget* l, const char* fmt, ...);

                int                 getNumPoints        (void);
                int                 getOffsetPoints     (void);
                void                setMarkers          (marker_t marker, uint64_t key);
                void                setZoom             (int start_index, int stop_index);

    private:

        /* -------------------------------------
         * Constants
         * ------------------------------------- */

        static const char* FILE_READER_NAME;
        static const int WINDOW_X_SIZE_INIT = 1000;
        static const int WINDOW_Y_SIZE_INIT = 400;

        /* -------------------------------------
         * Data
         * ------------------------------------- */

        static const char*  protocol_list[];

        Ordering<DataPlot*> plots;
        int                 plotKey;

        bool                pendingClose;
        const char*         outQName;

        GtkWidget*          window;
        GtkWidget*          plotRows;

        int                 maxNumPoints;           // used in setting the size of the indexed point list

        GtkWidget*          lockCheck;
        GtkWidget*          exportButton;

        GtkObject*          numPlotPointsAdj;
        GtkWidget*          numPlotPointsScroll;
        GtkWidget*          numPlotPointsSpinner;
        int                 numPlotPoints;          // used for the size of the plot

        GtkObject*          offsetPlotPointsAdj;
        GtkWidget*          offsetPlotPointsScroll; // used for the start of the plot
        GtkWidget*          offsetPlotPointsSpinner;
        int                 offsetPlotPoints;

        GtkWidget*          clearButton;


        /* -------------------------------------
         * Methods
         * ------------------------------------- */

                            Charter                 (CommandProcessor* cmd_proc, const char* obj_name, const char* _outq_name, int _max_num_points = MAX_DATA_POINTS);
                            ~Charter                (void);
                            
                int         showChartCmd            (int argc, char argv[][MAX_CMD_SIZE]);
                int         hideChartCmd            (int argc, char argv[][MAX_CMD_SIZE]);
                int         addPlotCmd              (int argc, char argv[][MAX_CMD_SIZE]);
                int         setPlotPointsCmd        (int argc, char argv[][MAX_CMD_SIZE]);
                int         setSigDigitsCmd         (int argc, char argv[][MAX_CMD_SIZE]);

        static  gboolean    deleteEvent             (GtkWidget* widget, GdkEvent* event, gpointer data);
        static  void        numPointsHandler        (GtkAdjustment *adjustment, gpointer user_data);
        static  void        offsetPointsHandler     (GtkAdjustment *adjustment, gpointer user_data);
        static  void        lockHandler             (GtkButton* button, gpointer user_data);
        static  void        exportHandler           (GtkButton* button, gpointer user_data);
        static  void        exportRangeHandler      (GtkButton* button, gpointer user_data);
        static  void        exportBlueHandler       (GtkButton* button, gpointer user_data);
        static  void        exportGreenHandler      (GtkButton* button, gpointer user_data);
        static  void        exportBluestepHandler   (GtkButton* button, gpointer user_data);
        static  void        exportGreenstepHandler  (GtkButton* button, gpointer user_data);
        static  void        selectHandler           (GtkButton* button, gpointer user_data);
        static  void        clearHandler            (GtkButton* button, gpointer user_data);

        static  void*       killThread              (void* parm);

                void        redrawPlots             (void);

};

/***********************
 DATAPLOT
************************/

class DataPlot
{
    public:

        /* -------------------------------------
         * Typedefs
         * ------------------------------------- */

        typedef struct {
            double  min;
            double  max;
            double  avg;
            int     num;
        } data_stat_t;

        /* -------------------------------------
         * Methods
         * ------------------------------------- */

                    DataPlot            (Charter* _charter, const char* _name, const char* inq_name, const char* outq_name, int max_num_point_points, int sig_digits);
                    ~DataPlot           (void);
        void        redraw              (void);
        void        lockData            (void);
        void        unlockData          (void);
        void        exportData          (void);
        void        exportData          (uint64_t key);
        void        exportMarker        (marker_t marker, bool with_increment);
        void        clearData           (void);
        void        setMarker           (marker_t marker, uint64_t key);
        void        incMarker           (marker_t marker);
        int         getLockedIndex      (uint64_t key);
        const char* getName             (void);
        GtkWidget*  getPlotBox          (void);

        /* -------------------------------------
         * Data
         * ------------------------------------- */

        bool                        selected;

    private:

        /* -------------------------------------
         * Typedef
         * ------------------------------------- */

        typedef MetricRecord::metric_t dp_t;
        
        /* -------------------------------------
         * Methods
         * ------------------------------------- */

        static  void*       dataThread      (void* parm);
        static  void*       drawThread      (void* parm);
        static  void        scaleYHandler   (GtkButton* button, gpointer user_data);
        static  gboolean    resizeHandler   (GtkWidget* widget, GtkAllocation* allocation, gpointer user_data);
        static  gboolean    mouseHandler    (GtkWidget* widget, GdkEvent* event, gpointer user_data);

        /* -------------------------------------
         * Data
         * ------------------------------------- */

        static const int PLOT_NAME_SIZE = 64;
        static const int WINDOW_X_SIZE_INIT = 1000;
        static const int WINDOW_Y_SIZE_INIT = 400;

        Charter*                    charter;
        char                        name[PLOT_NAME_SIZE];
        Subscriber*                 inQ;
        Publisher*                  outQ;
        MgOrdering<dp_t*>*          points;             // holds data_point_t*
        int                         maxNumPoints;       // given to points list as maximum size
        char                        valueFmt[10];       // format string for values
        int                         significantDigits;  // used in displaying values

        bool                        locked;
        double*                     lockedData;
        double*                     lockedKeys;
        unsigned char**             lockedPkts;
        int*                        lockedSizes;
        int                         lockedPoints;

        int                         plotWidth;
        int                         plotHeight;

        GtkWidget*                  plotBox;
        GtkWidget*                  canvas;
        GtkWidget*                  trace;
        GtkPlotData*                dataset;

        GtkWidget*                  blueMarker;
        GtkPlotData*                blueDataset;
        GtkWidget*                  greenMarker;
        GtkPlotData*                greenDataset;

        GtkWidget*                  viewMinLabel;
        GtkWidget*                  viewMaxLabel;
        GtkWidget*                  viewAvgLabel;
        GtkWidget*                  viewNumLabel;
        GtkWidget*                  totalMinLabel;
        GtkWidget*                  totalMaxLabel;
        GtkWidget*                  totalAvgLabel;
        GtkWidget*                  totalNumLabel;
        GtkWidget*                  blueKeyLabel;
        GtkWidget*                  blueValLabel;
        GtkWidget*                  greenKeyLabel;
        GtkWidget*                  greenValLabel;

        GtkWidget*                  fixYMinCheck;
        GtkWidget*                  fixYMaxCheck;
        GtkObject*                  scaleYMinAdj;
        GtkObject*                  scaleYMaxAdj;
        GtkWidget*                  scaleYMinSpin;
        GtkWidget*                  scaleYMaxSpin;

        double                      xRange[2];      // min, max
        double                      yRange[2];      // min, max

        double                      xBlue[2];       // x, x
        double                      yBlue;
        double                      xGreen[2];      // x, x
        double                      yGreen;

        int                         blueIndex;
        int                         greenIndex;

        bool                        showMarkers;

        data_stat_t                 viewStat;
        data_stat_t                 totalStat;

        pthread_mutex_t             dataMut;
        pthread_mutex_t             drawMut;
        pthread_cond_t              drawCond;
        pthread_t                   dataPid;
        pthread_t                   drawPid;
        bool                        active;
};

#endif  /* __CHARTER_HPP__ */
