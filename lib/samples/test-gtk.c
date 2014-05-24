#include <gtk/gtk.h>
#include <stdbool.h>

#include "../moose-api.h"

/*
 * Test applications to see if db search is fast enough.
 * It apparently is, but redrawing takes much longer than searching.
 */

enum {
    COLUMN_ARTIST = 0,
    COLUMN_ALBUM,
    COLUMN_TITLE,
    NUM_COLUMNS
};

typedef struct {
    MooseClient * client;
    MooseStore * store;
    GtkTreeModel * model;
    GtkTreeView * view;
    MoosePlaylist * song_buf;
    GTimer * profile_timer;
} EntryTag;

static GtkTreeModel * create_model(void)
{
    /* create list store */
    return GTK_TREE_MODEL(gtk_list_store_new(NUM_COLUMNS,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING)
                          );
}

static void update_view(EntryTag * tag, const char * search_text)
{
    GtkListStore * list_store = GTK_LIST_STORE(tag->model);
    gdouble select_time, gui_time, parse_time;
    GtkTreeIter iter;
    g_timer_start(tag->profile_timer);
    moose_playlist_clear(tag->song_buf);

    GTimer * parse_timer = g_timer_new();
    g_timer_start(parse_timer);
    char * query = moose_store_qp_parse(search_text, NULL, NULL);
    parse_time = g_timer_elapsed(parse_timer, NULL);


    moose_store_gw(tag->store, moose_store_search_to_stack(tag->store, query, true, tag->song_buf, -1));

    int found = moose_playlist_length(tag->song_buf);

    select_time = g_timer_elapsed(tag->profile_timer, NULL);

    //if (found == 0)
    //   return;

    g_timer_start(tag->profile_timer);

    GtkTreeModel * model = gtk_tree_view_get_model(GTK_TREE_VIEW(tag->view));
    g_object_ref(model); /* Make sure the model stays with us after the tree view unrefs it */
    gtk_tree_view_set_model(GTK_TREE_VIEW(tag->view), NULL); /* Detach model from view */


    gtk_list_store_clear(list_store);

    for (int i = 0; i < found; i++) {
        MooseSong * song = moose_playlist_at(tag->song_buf, i);
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter,
                           COLUMN_ARTIST, moose_song_get_tag(song, MOOSE_TAG_ARTIST),
                           COLUMN_ALBUM, moose_song_get_tag(song, MOOSE_TAG_ALBUM),
                           COLUMN_TITLE, moose_song_get_tag(song, MOOSE_TAG_TITLE),
                           -1);
    }

    moose_store_release(tag->store);

    gtk_tree_view_set_model(GTK_TREE_VIEW(tag->view), model); /* Re-attach model to view */
    g_object_unref(model);

    gui_time = g_timer_elapsed(tag->profile_timer, NULL);
    g_print("Timing: parse=%2.5fs + select=%2.5fs + gui_redraw=%2.5fs = %2.5fs (%-5d rows)\n\t%s\n",
            parse_time, select_time, gui_time, select_time + gui_time + parse_time, found, query);

    g_free(query);
}

static void on_entry_text_changed(GtkEditable * editable, gpointer user_data)
{
    gchar * entry_text = g_strdup(gtk_entry_get_text(GTK_ENTRY(editable)));

    if (entry_text != NULL) {
        g_strstrip(entry_text);
        update_view((EntryTag *)user_data, entry_text);
        g_free(entry_text);
    }
}

static gboolean window_closed(GtkWidget * widget, GdkEvent * event, gpointer user_data)
{
    (void)widget;
    (void)event;
    (void)user_data;
    gtk_main_quit();
    return FALSE;
}

static void on_client_update(MooseClient * self, MooseIdle event, void * user_data)
{
    (void)self;
    (void)event;
    g_print("Updating due to Queue/DB Update\n");
    EntryTag * tag = user_data;
    update_view(tag, "");
}

static EntryTag * setup_client(void)
{
    EntryTag * rc = NULL;
    gdouble client_setup = 0.0, db_setup = 0.0;
    GTimer * setup_timer = g_timer_new();
    MooseClient * client = moose_client_create(MOOSE_PM_COMMAND);
    moose_client_connect(client, NULL, "localhost", 6600, 2.0);

    if (client && moose_client_is_connected(client)) {
        client_setup = g_timer_elapsed(setup_timer, NULL);
        g_timer_start(setup_timer);
        MooseStore * store = moose_store_create_full(
            client, NULL, NULL, false, false
            );
        db_setup = g_timer_elapsed(setup_timer, NULL);

        if (store != NULL) {
            rc = g_new0(EntryTag, 1);
            rc->client = client;
            rc->store = store;
            rc->profile_timer = g_timer_new();
            rc->song_buf = moose_playlist_new_full(1000, NULL);
        }

        moose_client_signal_add_masked(client, "client-event",
                                       on_client_update, rc, MOOSE_IDLE_DATABASE | MOOSE_IDLE_QUEUE);
    }

    g_print("Setup Profiling: client-connect=%2.5fs + db-setup=%2.5fs = %2.6fs\n",
            client_setup, db_setup, client_setup + db_setup);
    return rc;
}

static void bringdown_client(EntryTag * tag)
{
    g_object_unref(tag->song_buf);
    moose_store_close(tag->store);
}

static void build_gui(EntryTag * tag)
{
    /* instance */
    tag->model = create_model();
    GtkWidget * wnd = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget * ent = gtk_entry_new();
    GtkWidget * tvw = gtk_tree_view_new_with_model(tag->model);
    GtkWidget * scw = gtk_scrolled_window_new(NULL, NULL);
    /* configure */
    g_signal_connect(ent, "changed", G_CALLBACK(on_entry_text_changed), tag);
    g_signal_connect(wnd, "delete-event", G_CALLBACK(window_closed), NULL);
    GtkCellRenderer * renderer;
    GtkTreeViewColumn * column;
    GtkTreeView * treeview = GTK_TREE_VIEW(tvw);
    tag->view = treeview;
    gtk_tree_view_set_rules_hint(treeview, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scw), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    gtk_tree_view_set_fixed_height_mode(treeview, true);
    static const char * column_desc[] = {
        "Artist",
        "Album",
        "Title"
    };

    /* columns */
    for (gsize i = 0; i < sizeof(column_desc) / sizeof(char *); i++) {
        renderer = gtk_cell_renderer_text_new();
        gtk_cell_renderer_text_set_fixed_height_from_font(GTK_CELL_RENDERER_TEXT(renderer), 1);
        column = gtk_tree_view_column_new_with_attributes(column_desc[i], renderer, "text", i, NULL);
        gtk_tree_view_column_set_min_width(column, 250);
        gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
        gtk_tree_view_column_set_sort_indicator(column, true);
        gtk_tree_view_append_column(treeview, column);
    }

    /* packing */
    gtk_container_add(GTK_CONTAINER(scw), tvw);
    gtk_box_pack_start(GTK_BOX(box), scw, true, true, 1);
    gtk_box_pack_start(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), false, false, 3);
    gtk_box_pack_start(GTK_BOX(box), ent, false, false, 1);
    gtk_container_add(GTK_CONTAINER(wnd), box);
    gtk_widget_show_all(wnd);
}

int main(int argc, char * argv[])
{
    EntryTag * tag = setup_client();
    gtk_init(&argc, &argv);
    build_gui(tag);
    gtk_main();
    bringdown_client(tag);
    return 0;
}
