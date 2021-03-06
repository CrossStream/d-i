/*
 * cdebconf gtk plugin to get random data
 *
 * Copyright © 2008 Jérémy Bobbio <lunar@debian.org>
 * See debian/copyright for license.
 *
 */

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <cdebconf/frontend.h>
#include <cdebconf/cdebconf_gtk.h>

#include <gtk/gtk.h>

#define MODULE "entropy"
#define FIFO "/var/run/random.fifo"
#define DEFAULT_KEYSIZE 2925

/* Here's the plugin! */
int cdebconf_gtk_handler_entropy(struct frontend * fe,
                                 struct question * question,
                                 GtkWidget * question_box);

struct entropy {
    struct frontend * fe;
    GtkWidget * progress_bar;
    GtkWidget * continue_button;
    GtkWidget * entry;
    guint64 keysize;
    const char * fifo;
    const char * success_template;
    guint64 bytes_read;
    int random_fd;
    GIOChannel * random_channel;
    guint random_source_id;
    int fifo_fd;
    guint8 random_byte;
};

static void destroy_entropy(struct entropy * entropy_data)
{
    if (0 < entropy_data->fifo_fd) {
        (void) close(entropy_data->fifo_fd);
    }
    if (NULL != entropy_data->fifo) {
        (void) unlink(entropy_data->fifo);
    }
    if (NULL == entropy_data->random_channel) {
        g_io_channel_unref(entropy_data->random_channel);
    }
    if (0 < entropy_data->random_fd) {
        (void) close(entropy_data->random_fd);
    }
    (void) munlock(&entropy_data->random_byte, sizeof (guint8));
    if (NULL != entropy_data->progress_bar) {
        g_object_unref(G_OBJECT(entropy_data->progress_bar));
    }
    if (NULL != entropy_data->entry) {
        g_object_unref(G_OBJECT(entropy_data->entry));
    }
    if (NULL != entropy_data->continue_button) {
        g_object_unref(G_OBJECT(entropy_data->continue_button));
    }
    g_free(entropy_data);
}

static struct entropy * init_entropy(struct frontend * fe,
                                     struct question * question)
{
    struct entropy * entropy_data;

    if (NULL == (entropy_data = g_malloc0(sizeof (struct entropy)))) {
        g_critical("g_malloc0 failed.");
        return NULL;
    }
    entropy_data->fe = fe;
    if (-1 == mlock(&entropy_data->random_byte, sizeof (guint8))) {
        g_critical("mlock failed: %s", strerror(errno));
        goto failed;
    }
    entropy_data->success_template = question_get_variable(
        question, "SUCCESS");
    if (NULL == entropy_data->success_template) {
        entropy_data->success_template = "debconf/entropy/success";
    }
    entropy_data->random_fd = open("/dev/random", O_RDONLY | O_NONBLOCK);
    if (-1 == entropy_data->random_fd) {
        g_critical("open random_fd failed: %s", strerror(errno));
        goto failed;
    }
    entropy_data->random_channel = g_io_channel_unix_new(entropy_data->random_fd);
    if (NULL == entropy_data->random_channel) {
        g_critical("g_io_channel_unix_new failed.");
        goto failed;
    }
    entropy_data->fifo = question_get_variable(question, "FIFO");
    if (NULL == entropy_data->fifo) {
        entropy_data->fifo = FIFO;
    }
    if (-1 == mkfifo(entropy_data->fifo, 0600)) {
        g_critical("mkfifo failed: %s", strerror(errno));
        goto failed;
    }
    entropy_data->fifo_fd = open(entropy_data->fifo, O_WRONLY);
    if (-1 == entropy_data->fifo_fd) {
        g_critical("open fifo_fd failed: %s", strerror(errno));
        goto failed;
    }
    return entropy_data;

failed:
    destroy_entropy(entropy_data);
    return NULL;
}

static void cleanup(GtkWidget * widget, struct entropy * entropy_data)
{
    if (0 != entropy_data->random_source_id) {
        g_source_remove(entropy_data->random_source_id);
    }
    destroy_entropy(entropy_data);
}

static void handle_continue(GtkWidget * button, struct entropy * entropy_data)
{
    cdebconf_gtk_set_answer_ok(entropy_data->fe);
}

static gboolean add_help_text(struct entropy * entropy_data,
                              GtkWidget * container)
{
    GtkWidget * view;
    GtkTextBuffer * buffer;
    GtkTextIter start;
    GtkTextIter end;
    GtkStyle * style;
    char * help_text;

    help_text = cdebconf_gtk_get_text(
        entropy_data->fe, "debconf/entropy/gtk/help",
        "You can help speed up the process by entering random characters on "
        "the keyboard or by moving the mouse randomly, or just wait until "
        "enough key data has been collected (which can take a long time).");
    /* XXX: check NULL! */
    view = gtk_text_view_new();
    /* XXX: check NULL! */
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
    gtk_text_buffer_set_text(buffer, help_text, -1 /* until '\0' */);
    g_free(help_text);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), DEFAULT_PADDING);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(view), DEFAULT_PADDING);
    gtk_text_buffer_create_tag(buffer, "italic", "style",
                               PANGO_STYLE_ITALIC, NULL);
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_apply_tag_by_name(buffer, "italic", &start, &end);
    style = gtk_widget_get_style(
        gtk_widget_get_toplevel(entropy_data->continue_button));
    gtk_widget_modify_base(view, GTK_STATE_NORMAL, style->bg);
    gtk_box_pack_start(GTK_BOX(container), view,
                       FALSE /* don't expand */, FALSE /* don't fill */,
                       DEFAULT_PADDING);
    return TRUE;
}

static gboolean add_action_text(struct entropy * entropy_data,
                                GtkWidget * container)
{
    GtkWidget * label;
    char * action_text;

    action_text = cdebconf_gtk_get_text(
        entropy_data->fe, "debconf/entropy/gtk/action",
        "Enter random characters or move mouse randomly");
    label = gtk_label_new(action_text);
    g_free(action_text);

    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_misc_set_alignment(GTK_MISC(label), 0 /* left */, 0 /* top */);
    gtk_box_pack_start(GTK_BOX(container), label, FALSE /* no expand */,
                       TRUE /* fill */, DEFAULT_PADDING);

    return TRUE;
}

static GtkWidget * create_entropy_widget(struct entropy * entropy_data)
{
    GtkWidget * vbox;
    GtkWidget * continue_button;
    GtkWidget * progress_bar;
    GtkWidget * entry;

    continue_button = cdebconf_gtk_create_continue_button(entropy_data->fe);
    if (NULL == continue_button) {
        g_critical("cdebconf_gtk_create_continue_button failed.");
        return NULL;
    }
    gtk_widget_set_can_focus(continue_button, FALSE);
    gtk_widget_set_sensitive(continue_button, FALSE);
    g_signal_connect(continue_button, "clicked", G_CALLBACK(handle_continue),
                     entropy_data);
    g_object_ref(G_OBJECT(continue_button));
    entropy_data->continue_button = continue_button;

    vbox = gtk_vbox_new(FALSE /* not homogenous */, DEFAULT_PADDING);
    if (NULL == vbox) {
        g_critical("gtk_vbox_new failed.");
        return NULL;
    }

    if (!add_help_text(entropy_data, vbox)) {
        g_critical("add_help_text failed.");
        return NULL;
    }

    if (NULL == (progress_bar = gtk_progress_bar_new())) {
        g_critical("gtk_progress_bar_new failed.");
        return NULL;
    }
    /* write a space to prepare progress bar height to receive the success
     * confirmation at the end. */
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), " ");
    gtk_box_pack_start(GTK_BOX(vbox), progress_bar, FALSE /* no expand */,
                       FALSE /* no fill */, DEFAULT_PADDING);
    g_object_ref(G_OBJECT(progress_bar));
    entropy_data->progress_bar = progress_bar;

    if (!add_action_text(entropy_data, vbox)) {
        g_critical("add_action_text failed.");
        return NULL;
    }

    entry = gtk_entry_new();
    if (NULL == entry) {
        g_critical("gtk_entry_new failed.");
        return NULL;
    }
    gtk_widget_set_can_default(entry, TRUE);
    gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE /* password style */);
    gtk_entry_set_activates_default(GTK_ENTRY(entry),
                                    TRUE /* activate on Enter */);
    gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE /* no expand */,
                       FALSE /* no fill */, DEFAULT_PADDING);
    g_object_ref(G_OBJECT(entry));
    entropy_data->entry = entry;

    g_signal_connect(vbox, "destroy", G_CALLBACK(cleanup), entropy_data);

    return vbox;
}

static void refresh_progress_bar(struct entropy * entropy_data)
{
    gdouble fraction;
    gchar * label;

    fraction = (gdouble) entropy_data->bytes_read /
               (gdouble) entropy_data->keysize;
    if (1.0 <= fraction) {
        label = cdebconf_gtk_get_text(
            entropy_data->fe, entropy_data->success_template,
            "Key data has been created successfully.");
    } else {
        label = g_strdup_printf("%2.0f%%", fraction * 100.0);
    }
    gdk_threads_enter();
    gtk_progress_bar_set_fraction(
        GTK_PROGRESS_BAR(entropy_data->progress_bar),
        fraction);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(entropy_data->progress_bar),
                              label);
    gdk_threads_leave();
    g_free(label);
}

static gboolean move_bytes(struct entropy * entropy_data)
{
    gssize n;

    while (entropy_data->bytes_read < entropy_data->keysize) {
        n = read(entropy_data->random_fd, &entropy_data->random_byte,
                 sizeof (guint8));
        if (1 > n) {
            if (EAGAIN == errno) {
                break;
            }
            g_critical("read failed: %s", strerror(errno));
            return FALSE;
        }
        n = write(entropy_data->fifo_fd, &entropy_data->random_byte,
                  sizeof (guint8));
        if (1 > n) {
            g_critical("write failed: %s", strerror(errno));
            return FALSE;
        }
        entropy_data->random_byte = 0;
        entropy_data->bytes_read++;
    }
    return TRUE;
}

static void allow_continue(struct entropy * entropy_data)
{
    gdk_threads_enter();
    gtk_widget_set_sensitive(entropy_data->entry, FALSE);
    gtk_widget_set_sensitive(entropy_data->continue_button, TRUE);
    gtk_widget_set_can_focus(entropy_data->continue_button, TRUE);
    gtk_widget_grab_default(entropy_data->continue_button);
    gdk_threads_leave();
}

static gboolean gather_entropy(GIOChannel *source, GIOCondition condition,
                               struct entropy * entropy_data)
{
    if (DC_NO_ANSWER != cdebconf_gtk_get_answer(entropy_data->fe)) {
        return FALSE;
    }
    if (G_IO_ERR == (condition & G_IO_ERR)) {
        return FALSE;
    }
    if (G_IO_IN != (condition & G_IO_IN)) {
        /* Wait for some data. */
	return TRUE;
    }

    if (!move_bytes(entropy_data)) {
	cdebconf_gtk_set_answer_notok(entropy_data->fe);
	return FALSE;
    }
    refresh_progress_bar(entropy_data);
    /* Reset text entry to prevent overflow. */
    gtk_entry_set_text(GTK_ENTRY(entropy_data->entry), "");

    if (entropy_data->bytes_read < entropy_data->keysize) {
	return TRUE;
    }

    allow_continue(entropy_data);
    return FALSE;
}

static gboolean set_keysize(struct entropy * entropy_data,
                            struct question * question) {
    const char * keysize_string;

    keysize_string = question_get_variable(question, "KEYSIZE");
    if (NULL == keysize_string) {
        entropy_data->keysize = DEFAULT_KEYSIZE;
        return TRUE;
    }
    entropy_data->keysize = g_ascii_strtoull(
        keysize_string, NULL /* don't get last parsed byte */,
        0 /* default base */);
    if (G_MAXUINT64 == entropy_data->keysize) {
        g_critical("keysize out of range");
        return FALSE;
    }
    if (0 == entropy_data->keysize) {
        g_critical("can't parse KEYSIZE");
        return FALSE;
    }
    return TRUE;
}

static void set_nothing(struct question * question, void * dummy)
{
    /* entropy questions do not put anything in the database */
    return;
}

int cdebconf_gtk_handler_entropy(struct frontend * fe,
                                 struct question * question,
                                 GtkWidget * question_box)
{
    struct entropy * entropy_data;
    GtkWidget * widget;

    if (!IS_QUESTION_SINGLE(question)) {
        g_critical("entropy plugin does not work alongside other questions.");
        return DC_NOTOK;
    }
    if (NULL == (entropy_data = init_entropy(fe, question))) {
        g_critical("init_entropy failed.");
        return DC_NOTOK;
    }
    if (!set_keysize(entropy_data, question)) {
        g_critical("set_keysize failed.");
        goto failed;
    }
    if (NULL == (widget = create_entropy_widget(entropy_data))) {
        g_critical("create_widget failed.");
        goto failed;
    }

    entropy_data->random_source_id = g_io_add_watch(
        entropy_data->random_channel, G_IO_IN | G_IO_ERR,
        (GIOFunc) gather_entropy, entropy_data);

    cdebconf_gtk_add_common_layout(fe, question, question_box, widget);

    gtk_widget_grab_focus(entropy_data->entry);
    gtk_widget_grab_default(entropy_data->entry);

    cdebconf_gtk_register_setter(fe, SETTER_FUNCTION(set_nothing), question,
                                 NULL);

    return DC_OK;

failed:
    destroy_entropy(entropy_data);
    return DC_NOTOK;
}

/* vim: et sw=4 si
 */
