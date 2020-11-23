#ifndef __SHELL_HPP__
#define __SHELL_HPP__

#include <gtk/gtk.h>
#include <gtkextra/gtkextra.h>

#include "rtap/core.h"
#include "rtap/ccsds.h"

class Shell: public CommandableObject
{
    public:
        
        /* -------------------------------------
         * Constants
         * ------------------------------------- */

        static const char* TYPE;

        /* -------------------------------------
         * Methods
         * ------------------------------------- */

        static CommandableObject* createObject (CommandProcessor* cmd_proc, const char* obj_name, int argc, char argv[][MAX_CMD_SIZE]);
        static int logHandler (const char* msg, int size, void* parm);

    private:
        
        /* -------------------------------------
         * Data
         * ------------------------------------- */

        static const char*      SHELL_LOGGER_NAME;
        static const int        WINDOW_X_SIZE_INIT = 1000;
        static const int        WINDOW_Y_SIZE_INIT = 400;

        bool                    active;
        bool                    pending_close;
        pthread_t               pid;
        
        Publisher*              logq_pub;        
        Subscriber*             logq_sub;
        okey_t                  msglog;

        MgList<const char*>     cmd_history;
        int                     cmd_index;
        
        GtkWidget*              ignore_radio;
        GtkWidget*              debug_radio;
        GtkWidget*              info_radio;
        GtkWidget*              warning_radio;
        GtkWidget*              error_radio;
        GtkWidget*              critical_radio;
        
        GtkWidget*              input_txtbox;
        GtkTextBuffer*          txt_buffer;
        GtkWidget*              output_txtview;
        GtkWidget*              message_window;
        GtkWidget*              window;

        /* -------------------------------------
         * Methods
         * ------------------------------------- */

                        Shell                   (CommandProcessor* cmd_proc, const char* obj_name, const char* _logq_name);
                        ~Shell                  (void);

        static void*    logThread               (void* parm);

        static gboolean deleteEvent             (GtkWidget* widget, GdkEvent* event, gpointer data);
        static void     cmdEntry                (GtkWidget* widget, gpointer data);
        static gboolean cmdKeyHandler           (GtkWidget* widget, GdkEvent* event, gpointer data);
        static gboolean messageRadioHandler     (GtkToggleButton* togglebutton, gpointer user_data);        
};

#endif  /* __SHELL_HPP__ */
