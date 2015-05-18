#include "moose-list-model.h"
#include <gtk/gtk.h>

typedef struct _MooseListModelClass { GObjectClass parent_class; } MooseListModelClass;

typedef struct _MooseListModelRecord {
    /* data - you can extend this */
    gchar *name;
    guint year_born;

    /* pos within the array */
    guint pos;
} MooseListModelRecord;

/* Add types here */
typedef struct _MooseListModelPrivate {
    gint n_columns;
    guint num_rows;

    GType column_types[MOOSE_LIST_MODEL_N_COLUMNS];
    MooseListModelRecord **rows;

    /* Random integer to check whether an iter belongs to our model.
     * Used by Gtk+ to invalidate old Iterators. */
    gint stamp;
} MooseListModelPrivate;

/* Object related functions */
static void moose_list_model_init(MooseListModel *pkg_tree);
static void moose_list_model_class_init(MooseListModelClass *klass);
static void moose_list_model_tree_model_init(GtkTreeModelIface *iface);
static void moose_list_model_finalize(GObject *object);

/* GtkTreeModel related functions */
static GtkTreeModelFlags moose_list_model_get_flags(GtkTreeModel *tree_model);
static gint moose_list_model_get_n_columns(GtkTreeModel *tree_model);
static GType moose_list_model_get_column_type(GtkTreeModel *tree_model, gint index);
static gboolean moose_list_model_get_iter(GtkTreeModel *tree_model,
                                          GtkTreeIter *iter,
                                          GtkTreePath *path);
static GtkTreePath *moose_list_model_get_path(GtkTreeModel *tree_model,
                                              GtkTreeIter *iter);
static void moose_list_model_get_value(GtkTreeModel *tree_model,
                                       GtkTreeIter *iter,
                                       gint column,
                                       GValue *value);
static gboolean moose_list_model_iter_next(GtkTreeModel *tree_model, GtkTreeIter *iter);
static gboolean moose_list_model_iter_children(GtkTreeModel *tree_model,
                                               GtkTreeIter *iter,
                                               GtkTreeIter *parent);
static gboolean moose_list_model_iter_has_child(GtkTreeModel *tree_model,
                                                GtkTreeIter *iter);
static gint moose_list_model_iter_n_children(GtkTreeModel *tree_model, GtkTreeIter *iter);
static gboolean moose_list_model_iter_nth_child(GtkTreeModel *tree_model,
                                                GtkTreeIter *iter,
                                                GtkTreeIter *parent,
                                                gint n);
static gboolean moose_list_model_iter_parent(GtkTreeModel *tree_model,
                                             GtkTreeIter *iter,
                                             GtkTreeIter *child);

/* GObject stuff - nothing to worry about */
static GObjectClass *PARENT_CLASS = NULL;

/* Skip most of the boilerplate (like get_type) */
G_DEFINE_TYPE_EXTENDED(MooseListModel, moose_list_model, G_TYPE_OBJECT, 0,
                       G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL,
                                             moose_list_model_tree_model_init));

#define MOOSE_LIST_MODEL_TYPE (moose_list_model_get_type())

/* Add the o->priv member */
#define MOOSE_LIST_MODEL_GET_PRIVATE(o) \
    (G_TYPE_INSTANCE_GET_PRIVATE((o), MOOSE_LIST_MODEL_TYPE, MooseListModelPrivate))

/*****************************************************************************
*
*  moose_list_model_class_init: more boilerplate GObject/GType stuff.
*                          Init callback for the type system,
*                          called once when our new class is created.
*
*****************************************************************************/

static void moose_list_model_class_init(MooseListModelClass *klass) {
    GObjectClass *object_class;
    PARENT_CLASS = (GObjectClass *)g_type_class_peek_parent(klass);
    object_class = (GObjectClass *)klass;
    object_class->finalize = moose_list_model_finalize;
    g_type_class_add_private(object_class, sizeof(MooseListModelPrivate));
}

/*****************************************************************************
*
*  moose_list_model_tree_model_init: init callback for the interface registration
*                               in moose_list_model_get_type. Here we override
*                               the GtkTreeModel interface functions that
*                               we implement.
*
*****************************************************************************/

static void moose_list_model_tree_model_init(GtkTreeModelIface *iface) {
    iface->get_flags = moose_list_model_get_flags;
    iface->get_n_columns = moose_list_model_get_n_columns;
    iface->get_column_type = moose_list_model_get_column_type;
    iface->get_iter = moose_list_model_get_iter;
    iface->get_path = moose_list_model_get_path;
    iface->get_value = moose_list_model_get_value;
    iface->iter_next = moose_list_model_iter_next;
    iface->iter_children = moose_list_model_iter_children;
    iface->iter_has_child = moose_list_model_iter_has_child;
    iface->iter_n_children = moose_list_model_iter_n_children;
    iface->iter_nth_child = moose_list_model_iter_nth_child;
    iface->iter_parent = moose_list_model_iter_parent;
}

/*****************************************************************************
*
*  moose_list_model_init: this is called everytime a new custom list object
*                    instance is created (we do that in moose_list_model_new).
*                    Initialise the list structure's fields here.
*
*****************************************************************************/

static void moose_list_model_init(MooseListModel *self) {
    self->priv = MOOSE_LIST_MODEL_GET_PRIVATE(self);
    self->priv->n_columns = MOOSE_LIST_MODEL_N_COLUMNS;
    self->priv->column_types[0] = G_TYPE_POINTER; /* moose_list_model_COL_RECORD    */
    self->priv->column_types[1] = G_TYPE_STRING;  /* moose_list_model_COL_NAME      */
    self->priv->column_types[2] = G_TYPE_UINT;    /* moose_list_model_COL_YEAR_BORN */

    self->priv->num_rows = 0;
    self->priv->rows = NULL;

    /* Random int to check whether an iter belongs to our model */
    self->priv->stamp = g_random_int();
}

/*****************************************************************************
*
*  moose_list_model_finalize: this is called just before a custom list is
*                        destroyed. Free dynamically allocated memory here.
*
*****************************************************************************/

static void moose_list_model_finalize(GObject *object) {
    MooseListModelPrivate *priv = MOOSE_LIST_MODEL_GET_PRIVATE(object);
    if(priv->rows) {
        for(gsize i = 0; i < priv->num_rows; ++i) {
            MooseListModelRecord *record = priv->rows[i];
            g_free(record->name);
            g_free(record);
        }
        g_free(priv->rows);
        priv->rows = NULL;
    }

    /* must chain up - finalize parent */
    (*PARENT_CLASS->finalize)(object);
}

/*****************************************************************************
*
*  moose_list_model_get_flags: tells the rest of the world whether our tree model
*                         has any special characteristics. In our case,
*                         we have a list model (instead of a tree), and each
*                         tree iter is valid as long as the row in question
*                         exists, as it only contains a pointer to our struct.
*
*****************************************************************************/

static GtkTreeModelFlags moose_list_model_get_flags(GtkTreeModel *tree_model) {
    g_return_val_if_fail(MOOSE_IS_LIST_MODEL(tree_model), (GtkTreeModelFlags)0);
    return GTK_TREE_MODEL_LIST_ONLY | GTK_TREE_MODEL_ITERS_PERSIST;
}

/*****************************************************************************
*
*  moose_list_model_get_n_columns: tells the rest of the world how many data
*                             columns we export via the tree model interface
*
*****************************************************************************/

static gint moose_list_model_get_n_columns(GtkTreeModel *tree_model) {
    g_return_val_if_fail(MOOSE_IS_LIST_MODEL(tree_model), 0);
    return MOOSE_LIST_MODEL_GET_PRIVATE(tree_model)->n_columns;
}

/*****************************************************************************
*
*  moose_list_model_get_column_type: tells the rest of the world which type of
*                               data an exported model column contains
*
*****************************************************************************/

static GType moose_list_model_get_column_type(GtkTreeModel *tree_model, gint index) {
    g_return_val_if_fail(MOOSE_IS_LIST_MODEL(tree_model), G_TYPE_INVALID);
    g_return_val_if_fail(
        index >= 0 && index < (gint)MOOSE_LIST_MODEL_GET_PRIVATE(tree_model)->n_columns,
        G_TYPE_INVALID);
    return MOOSE_LIST_MODEL_GET_PRIVATE(tree_model)->column_types[index];
}

/*****************************************************************************
*
*  moose_list_model_get_iter: converts a tree path (physical position) into a
*                        tree iter structure (the content of the iter
*                        fields will only be used internally by our model).
*                        We simply store a pointer to our MooseListModelRecord
*                        structure that represents that row in the tree iter.
*
*****************************************************************************/

static gboolean moose_list_model_get_iter(GtkTreeModel *tree_model,
                                          GtkTreeIter *iter,
                                          GtkTreePath *path) {
    g_assert(MOOSE_IS_LIST_MODEL(tree_model));
    g_assert(path != NULL);

    MooseListModel *self = MOOSE_LIST_MODEL(tree_model);

    /* we do not allow children */
    g_assert(gtk_tree_path_get_depth(path) == 1);

    /* the n-th top level row */
    gint n = gtk_tree_path_get_indices(path)[0];
    if(n >= (glong)self->priv->num_rows || n < 0) {
        return FALSE;
    }

    MooseListModelRecord *record = self->priv->rows[n];

    g_assert(record != NULL);

    /* We simply store a pointer to our custom record in the iter */
    iter->stamp = self->priv->stamp;
    iter->user_data = record;
    iter->user_data2 = NULL; /* unused */
    iter->user_data3 = NULL; /* unused */
    return TRUE;
}

/*****************************************************************************
*
*  moose_list_model_get_path: converts a tree iter into a tree path (ie. the
*                        physical position of that row in the list).
*
*****************************************************************************/

static GtkTreePath *moose_list_model_get_path(GtkTreeModel *tree_model,
                                              GtkTreeIter *iter) {
    g_return_val_if_fail(MOOSE_IS_LIST_MODEL(tree_model), NULL);
    g_return_val_if_fail(iter != NULL, NULL);
    g_return_val_if_fail(iter->user_data != NULL, NULL);

    MooseListModelRecord *record = (MooseListModelRecord *)iter->user_data;
    return gtk_tree_path_new_from_indices(record->pos, -1);
}

/*****************************************************************************
*
*  moose_list_model_get_value: Returns a row's exported data columns
*                         (_get_value is what gtk_tree_model_get uses)
*
*****************************************************************************/

static void moose_list_model_get_value(GtkTreeModel *tree_model,
                                       GtkTreeIter *iter,
                                       gint column,
                                       GValue *value) {
    MooseListModel *self = MOOSE_LIST_MODEL(tree_model);

    g_return_if_fail(MOOSE_IS_LIST_MODEL(tree_model));
    g_return_if_fail(iter != NULL);
    g_return_if_fail(column < self->priv->n_columns);

    g_value_init(value, MOOSE_LIST_MODEL_GET_PRIVATE(tree_model)->column_types[column]);

    MooseListModelRecord *record = (MooseListModelRecord *)iter->user_data;

    g_return_if_fail(record != NULL);

    if(record->pos >= self->priv->num_rows) {
        g_return_if_reached();
    }

    switch(column) {
    case MOOSE_LIST_MODEL_COL_RECORD:
        g_value_set_pointer(value, record);
        break;

    case MOOSE_LIST_MODEL_COL_NAME:
        g_value_set_string(value, record->name);
        break;

    case MOOSE_LIST_MODEL_COL_YEAR_BORN:
        g_value_set_uint(value, record->year_born);
        break;
    }
}

/*****************************************************************************
*
*  moose_list_model_iter_next: Takes an iter structure and sets it to point
*                         to the next row. This is the most called function.
*
*****************************************************************************/

static gboolean moose_list_model_iter_next(GtkTreeModel *tree_model, GtkTreeIter *iter) {
    g_return_val_if_fail(MOOSE_IS_LIST_MODEL(tree_model), FALSE);

    if(iter == NULL || iter->user_data == NULL) {
        return FALSE;
    }

    MooseListModel *self = MOOSE_LIST_MODEL(tree_model);
    MooseListModelRecord *record = (MooseListModelRecord *)iter->user_data;

    /* Is this the last record in the list? */
    if((record->pos + 1) >= self->priv->num_rows) {
        return FALSE;
    }

    MooseListModelRecord *nextrecord = self->priv->rows[(record->pos + 1)];
    g_assert(nextrecord != NULL);

    iter->stamp = self->priv->stamp;
    iter->user_data = nextrecord;
    return TRUE;
}

/*****************************************************************************
*
*  moose_list_model_iter_children: Returns TRUE or FALSE depending on whether
*                             the row specified by 'parent' has any children.
*                             If it has children, then 'iter' is set to
*                             point to the first child. Special case: if
*                             'parent' is NULL, then the first top-level
*                             row should be returned if it exists.
*
*****************************************************************************/

static gboolean moose_list_model_iter_children(GtkTreeModel *tree_model,
                                               GtkTreeIter *iter,
                                               GtkTreeIter *parent) {
    g_return_val_if_fail(parent == NULL || parent->user_data != NULL, FALSE);

    /* this is a list, nodes have no children */
    if(parent) {
        return FALSE;
    }

    g_return_val_if_fail(MOOSE_IS_LIST_MODEL(tree_model), FALSE);

    /* parent == NULL is a special case; we need to return the first top-level row */
    MooseListModel *self = MOOSE_LIST_MODEL(tree_model);

    /* No rows => no first row */
    if(self->priv->num_rows == 0) {
        return FALSE;
    }

    /* Set iter to first item in list */
    iter->stamp = self->priv->stamp;
    iter->user_data = self->priv->rows[0];
    return TRUE;
}

/*****************************************************************************
*
*  moose_list_model_iter_has_child: Returns TRUE or FALSE depending on whether
*                              the row specified by 'iter' has any children.
*                              We only have a list and thus no children.
*
*****************************************************************************/

static gboolean moose_list_model_iter_has_child(G_GNUC_UNUSED GtkTreeModel *tree_model,
                                                G_GNUC_UNUSED GtkTreeIter *iter) {
    return FALSE;
}

/*****************************************************************************
*
*  moose_list_model_iter_n_children: Returns the number of children the row
*                               specified by 'iter' has. This is usually 0,
*                               as we only have a list and thus do not have
*                               any children to any rows. A special case is
*                               when 'iter' is NULL, in which case we need
*                               to return the number of top-level nodes,
*                               ie. the number of rows in our list.
*
*****************************************************************************/

static gint moose_list_model_iter_n_children(GtkTreeModel *tree_model,
                                             GtkTreeIter *iter) {
    g_return_val_if_fail(MOOSE_IS_LIST_MODEL(tree_model), -1);
    g_return_val_if_fail(iter == NULL || iter->user_data != NULL, FALSE);

    MooseListModel *self = MOOSE_LIST_MODEL(tree_model);

    /* special case: if iter == NULL, return number of top-level rows */
    if(iter == NULL) {
        return self->priv->num_rows;
    } else {
        return 0; /* otherwise, this is easy again for a list */
    }
}

/*****************************************************************************
*
*  moose_list_model_iter_nth_child: If the row specified by 'parent' has any
*                              children, set 'iter' to the n-th child and
*                              return TRUE if it exists, otherwise FALSE.
*                              A special case is when 'parent' is NULL, in
*                              which case we need to set 'iter' to the n-th
*                              row if it exists.
*
*****************************************************************************/

static gboolean moose_list_model_iter_nth_child(GtkTreeModel *tree_model,
                                                GtkTreeIter *iter,
                                                GtkTreeIter *parent,
                                                gint n) {
    g_return_val_if_fail(MOOSE_IS_LIST_MODEL(tree_model), FALSE);

    MooseListModel *self = MOOSE_LIST_MODEL(tree_model);

    /* a list has only top-level rows */
    if(parent) {
        return FALSE;
    }

    /* special case: if parent == NULL, set iter to n-th top-level row */
    if(n >= (glong)self->priv->num_rows) {
        return FALSE;
    }

    MooseListModelRecord *record = self->priv->rows[n];
    iter->user_data = record;
    iter->stamp = self->priv->stamp;

    g_assert(record != NULL);
    return TRUE;
}

/*****************************************************************************
*
*  moose_list_model_iter_parent: Point 'iter' to the parent node of 'child'. As
*                           we have a list and thus no children and no
*                           parents of children, we can just return FALSE.
*
*****************************************************************************/

static gboolean moose_list_model_iter_parent(G_GNUC_UNUSED GtkTreeModel *tree_model,
                                             G_GNUC_UNUSED GtkTreeIter *iter,
                                             G_GNUC_UNUSED GtkTreeIter *child) {
    return FALSE;
}

/*****************************************************************************
*
*  moose_list_model_new:  This is what you use in your own code to create a
*                    new custom list tree model for you to use.
*
*****************************************************************************/

MooseListModel *moose_list_model_new(void) {
    MooseListModel *newcustomlist = g_object_new(MOOSE_LIST_MODEL_TYPE, NULL);
    g_assert(newcustomlist != NULL);
    return newcustomlist;
}

/*****************************************************************************
*
*  moose_list_model_append_record:  Empty lists are boring. This function can
*                              be used in your own code to add rows to the
*                              list. Note how we emit the "row-inserted"
*                              signal after we have appended the row
*                              internally, so the tree view and other
*                              interested objects know about the new row.
*
*****************************************************************************/

void moose_list_model_append_record(MooseListModel *self, const gchar *name,
                                    guint year_born) {
    GtkTreeIter iter;
    GtkTreePath *path;
    gulong newsize;
    guint pos;
    MooseListModelRecord *newrecord;

    g_return_if_fail(MOOSE_IS_LIST_MODEL(self));
    g_return_if_fail(name != NULL);

    pos = self->priv->num_rows++;

    newsize = self->priv->num_rows * sizeof(MooseListModelRecord *);
    self->priv->rows = g_realloc(self->priv->rows, newsize);

    newrecord = g_new0(MooseListModelRecord, 1);
    newrecord->name = g_strdup(name);
    newrecord->year_born = year_born;

    self->priv->rows[pos] = newrecord;
    newrecord->pos = pos;

    /* inform the tree view and other interested objects
     *  (e.g. tree row references) that we have inserted
     *  a new row, and where it was inserted */
    path = gtk_tree_path_new_from_indices(newrecord->pos, -1);
    moose_list_model_get_iter(GTK_TREE_MODEL(self), &iter, path);
    gtk_tree_model_row_inserted(GTK_TREE_MODEL(self), path, &iter);
    gtk_tree_path_free(path);
}
