/*-----------------------------------------------------------------------------
--
-- Module Name: Shell.cpp
--
-- Description:
--
--   Encapsulation of GTK Shell Application
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
#include <math.h>
#include <errno.h>
#include <semaphore.h>

#include "rtap/core.h"
#include "rtap/ccsds.h"

#include "Shell.h"

/******************************************************************************
 STATIC DATA
 ******************************************************************************/

const char* Shell::TYPE = "Shell";
const char* Shell::SHELL_LOGGER_NAME = "shelllog";

/******************************************************************************
 PUBLIC FUNCTIONS
 ******************************************************************************/

/*--------------------------------------------------------------------------------------
  -- Constructor  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/

Shell::Shell(CommandProcessor* cmd_proc, const char* obj_name, const char* _logq_name): 
    CommandableObject(cmd_proc, obj_name, TYPE),
    cmd_history(true)   // is array
{
    assert(_logq_name);

    /* Initialize Class Components */
    active          = true;
    pending_close   = false;
    msglog          = LogLib::createLog(CRITICAL, logHandler, this);
    cmd_index       = 0;
    

    /* Create Log Queue Interface */
    logq_pub = new Publisher(_logq_name);
    logq_sub = new Subscriber(_logq_name);
    
    /* Create GTK Components */
    gdk_threads_enter();
    {
        /* Create Default Font */
        PangoFontDescription* font_desc = pango_font_description_from_string ("DejaVu Sans Mono");

        /* Build Toggle Buttons */
        ignore_radio    = gtk_radio_button_new_with_label (NULL, 						"Ignore");
        debug_radio 	= gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (ignore_radio), 	"Debug");
        info_radio 	= gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (debug_radio), 		"Info");
        warning_radio 	= gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (info_radio), 		"Warning");
        error_radio     = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (warning_radio), 	"Error");
        critical_radio 	= gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (error_radio),		"Critical");

        /* Initialize Toggle Buttons */
        log_lvl_t lvl = LogLib::getLevel(msglog);
        if     (lvl == IGNORE)      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(ignore_radio), TRUE);
        else if(lvl == DEBUG)       gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(debug_radio), TRUE);
        else if(lvl == INFO)        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(info_radio), TRUE);
        else if(lvl == WARNING)     gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(warning_radio), TRUE);
        else if(lvl == ERROR)       gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(error_radio), TRUE);
        else if(lvl == CRITICAL)    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(critical_radio), TRUE);
        else                        mlog(RAW, "Unable to configure message panel log level to %d\n", lvl);

        /* Build Message Panel */
        GtkWidget* log_box = gtk_vbox_new (TRUE, 1);
        gtk_box_pack_start (GTK_BOX (log_box), ignore_radio,	TRUE, TRUE, 1);
        gtk_box_pack_start (GTK_BOX (log_box), debug_radio, 	TRUE, TRUE, 1);
        gtk_box_pack_start (GTK_BOX (log_box), info_radio, 	TRUE, TRUE, 1);
        gtk_box_pack_start (GTK_BOX (log_box), warning_radio, 	TRUE, TRUE, 1);
        gtk_box_pack_start (GTK_BOX (log_box), error_radio, 	TRUE, TRUE, 1);
        gtk_box_pack_start (GTK_BOX (log_box), critical_radio, 	TRUE, TRUE, 1);
        GtkWidget* message_panel = gtk_frame_new("Messages");
        gtk_container_add(GTK_CONTAINER(message_panel), log_box);

        /* Build Log Panel */
        input_txtbox   = gtk_entry_new();
        txt_buffer     = gtk_text_buffer_new(NULL);
        output_txtview = gtk_text_view_new_with_buffer(txt_buffer);
        message_window = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (message_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(message_window), output_txtview);
        gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(output_txtview), FALSE);
        gtk_text_view_set_editable(GTK_TEXT_VIEW(output_txtview), FALSE);
        gtk_widget_modify_font (output_txtview, font_desc);
        GtkWidget* log_panel = gtk_vbox_new(FALSE, 1);
        gtk_box_pack_start(GTK_BOX(log_panel), message_window, TRUE, TRUE, 1);
        gtk_box_pack_start(GTK_BOX(log_panel), input_txtbox, FALSE, TRUE, 1);

        /* Build Window */
        window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_container_set_border_width(GTK_CONTAINER(window), 10);
        gtk_window_set_default_size(GTK_WINDOW(window), WINDOW_X_SIZE_INIT,WINDOW_Y_SIZE_INIT);

        /* Layout Window */
        GtkWidget* main_box = gtk_hbox_new(FALSE, 1);
        gtk_box_pack_start(GTK_BOX(main_box), message_panel, FALSE, TRUE, 1);
        gtk_box_pack_start(GTK_BOX(main_box), log_panel, TRUE, TRUE, 1);
        GtkWidget* window_box = gtk_vbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(window_box), main_box, TRUE, TRUE, 1);
        gtk_container_add(GTK_CONTAINER(window), window_box);

        /* Attach Handlers */
        g_signal_connect(window,                    "delete-event",         G_CALLBACK(deleteEvent),           this);
        g_signal_connect(input_txtbox,              "activate",             G_CALLBACK(cmdEntry),              this);
        g_signal_connect(input_txtbox,              "key-press-event",      G_CALLBACK(cmdKeyHandler),         this);
        g_signal_connect(ignore_radio,              "toggled",              G_CALLBACK(messageRadioHandler),   this);
        g_signal_connect(debug_radio,               "toggled",              G_CALLBACK(messageRadioHandler),   this);
        g_signal_connect(ignore_radio,              "toggled",              G_CALLBACK(messageRadioHandler),   this);
        g_signal_connect(info_radio,                "toggled",              G_CALLBACK(messageRadioHandler),   this);
        g_signal_connect(warning_radio,             "toggled",              G_CALLBACK(messageRadioHandler),   this);
        g_signal_connect(error_radio,               "toggled",              G_CALLBACK(messageRadioHandler),   this);
        g_signal_connect(critical_radio,            "toggled",              G_CALLBACK(messageRadioHandler),   this);

        /* Show Widgets */
        gtk_widget_show_all(window);
    }
    gdk_threads_leave();

    /* Spawn Log Thread */
    pthread_create(&pid, NULL, logThread, this);
}

/*--------------------------------------------------------------------------------------
  -- Destructor  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
Shell::~Shell(void)
{
    /* Stop Shell Thread */
    active = false;
    if(pthread_join(pid, NULL) != 0)
    {
        mlog(CRITICAL, "Unable to join shell thread: %s\n", strerror(errno));
    }

    /* Delete Log */
    delete logq_pub;
    delete logq_sub;

    /* Destroy Window */
    gdk_threads_enter();
    {
        gtk_widget_destroy(GTK_WIDGET(window));
    }
    gdk_threads_leave();
}

/*--------------------------------------------------------------------------------------
  -- createCmd  -
  --
  --   Notes: Command Processor Command
  -------------------------------------------------------------------------------------*/
CommandableObject* Shell::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    const char* logq_name = StringLib::checkNullStr(argv[0]);

    if(logq_name == NULL)
    {
        mlog(CRITICAL, "Shell requires message queue name for logging\n");
        return NULL;
    }

    return new Shell(cmd_proc, name, logq_name);
}

/*--------------------------------------------------------------------------------------
  -- logHandler
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
int Shell::logHandler(const char* msg, int size, void* parm)
{
    Shell* shell = (Shell*)parm;
    return shell->logq_pub->postCopy(msg, size);
}

/******************************************************************************
 PRIVATE FUNCTIONS
 ******************************************************************************/

/*--------------------------------------------------------------------------------------
  -- Log Thread  -
  --
  --   Notes:
  -------------------------------------------------------------------------------------*/
void* Shell::logThread(void* parm)
{
    Shell* shell = (Shell*)parm;

    if(shell == NULL || shell->logq_sub == NULL) return NULL;

    char* msg_buffer = new char[LogLib::MAX_LOG_ENTRY_SIZE];

    while(shell->active)
    {
        /* Block on read of input message queue */
        memset(msg_buffer, 0, LogLib::MAX_LOG_ENTRY_SIZE);
        int size = shell->logq_sub->receiveCopy(msg_buffer, LogLib::MAX_LOG_ENTRY_SIZE, 1000);
        if(size > 0)
        {
            gdk_threads_enter();
            {
                GtkTextIter end;
                gtk_text_buffer_get_end_iter (shell->txt_buffer, &end);
                GtkTextMark* mark = gtk_text_buffer_create_mark(shell->txt_buffer, "end", &end, true);
                gtk_text_buffer_insert(shell->txt_buffer, &end, msg_buffer, -1);
                gtk_text_buffer_get_end_iter (shell->txt_buffer, &end);
                if(msg_buffer[size - 2] != '\n') gtk_text_buffer_insert(shell->txt_buffer, &end, "\n", -1);
                gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(shell->output_txtview), mark);
            }
            gdk_threads_leave();
        }
    }

    return NULL;
}

/*--------------------------------------------------------------------------------------
  -- deleteEvent  -
  -
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
gboolean Shell::deleteEvent(GtkWidget* widget, GdkEvent* event, gpointer data)
{
    (void)widget;
    (void)event;
    (void)data;

    return FALSE;
}

/*--------------------------------------------------------------------------------------
  -- cmdEntry  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
void Shell::cmdEntry(GtkWidget* widget, gpointer data)
{
    Shell* shell = (Shell*)data;

    const char* cmdstr = StringLib::duplicate(gtk_entry_get_text(GTK_ENTRY(widget)));
    shell->cmdProc->postCommand(cmdstr);    
    shell->cmd_history.add(cmdstr);
    shell->cmd_index = shell->cmd_history.length();

    gtk_entry_set_text(GTK_ENTRY(widget), "");
}

/*--------------------------------------------------------------------------------------
  -- cmdKeyHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
#define UP_ARROW 65362
#define DOWN_ARROW 65364
gboolean Shell::cmdKeyHandler(GtkWidget* widget, GdkEvent* event, gpointer data)
{
    Shell* shell = (Shell*)data;
    (void)shell;
    (void)widget;

    if(event->key.keyval == UP_ARROW)
    {
        if(shell->cmd_index > 0)
        {
            shell->cmd_index--;
            const char* display_str = shell->cmd_history[shell->cmd_index];
            gtk_entry_set_text(GTK_ENTRY(widget), display_str);
        }
        
        return true;
    }
    else if(event->key.keyval == DOWN_ARROW)
    {
        if(shell->cmd_index < shell->cmd_history.length())
        {
            shell->cmd_index++;
        }
        
        if(shell->cmd_index == shell->cmd_history.length())
        {
            gtk_entry_set_text(GTK_ENTRY(widget), "");
        }
        else
        {
            const char* display_str = shell->cmd_history[shell->cmd_index];
            gtk_entry_set_text(GTK_ENTRY(widget), display_str);
        }
        
        return true;
    }
    else
    {
        return false;
    }
}

/*--------------------------------------------------------------------------------------
  -- messageRadioHandler  -
  --
  --   Notes: called in context of GTK Thread
  -------------------------------------------------------------------------------------*/
gboolean Shell::messageRadioHandler(GtkToggleButton *togglebutton, gpointer user_data)
{
    if(gtk_toggle_button_get_active(togglebutton) == TRUE)
    {
        const char* button_name = gtk_button_get_label(GTK_BUTTON(togglebutton));
        Shell* shell = (Shell*)user_data;

        log_lvl_t lvl;
        if(LogLib::str2lvl(button_name, &lvl) == true)
        {
            LogLib::setLevel(shell->msglog, lvl);
            return TRUE;
        }
    }

    return FALSE;
}

