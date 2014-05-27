#ifndef MOOSE_LIST_MODEL_H
#define MOOSE_LIST_MODEL_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

/**
 * SECTION: moose-list-model
 * @short_description: A custom ListModel
 *
 * The #MooseListModel is a faster ListModel.
 */

/* Some boilerplate GObject defines */
#define MOOSE_TYPE_LIST_MODEL \
    (moose_list_model_get_type())
#define MOOSE_LIST_MODEL(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), MOOSE_TYPE_LIST_MODEL, MooseListModel))
#define MOOSE_LIST_MODEL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), MOOSE_TYPE_LIST_MODEL, MooseListModelClass))
#define MOOSE_IS_LIST_MODEL(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), MOOSE_TYPE_LIST_MODEL))
#define MOOSE_IS_LIST_MODEL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), MOOSE_TYPE_LIST_MODEL))
#define MOOSE_LIST_MODEL_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), MOOSE_TYPE_LIST_MODEL, MooseListModelClass))

/* The data columns that we export via the tree model interface */
enum {
    MOOSE_LIST_MODEL_COL_RECORD = 0,
    MOOSE_LIST_MODEL_COL_NAME,
    MOOSE_LIST_MODEL_COL_YEAR_BORN,
    MOOSE_LIST_MODEL_N_COLUMNS,
};

/* MooseListModel: this structure contains everything we need for our
 *             model implementation. You can add extra fields to
 *             this structure, e.g. hashtables to quickly lookup
 *             rows or whatever else you might need, but it is
 *             crucial that 'parent' is the first member of the
 *             structure.
 */
typedef struct _MooseListModel {
    /* this must be the first member for memory layout. */
    GObject parent;
    struct _MooseListModelPrivate * priv;
} MooseListModel;

GType moose_list_model_get_type(void);


//              PUBLIC API                     //


/**
 * moose_list_model_new:
 *
 * Allocates a new #MooseListModel.
 *
 * Return value: a new #MooseListModel.
 */
MooseListModel * moose_list_model_new(void);

/**
 * moose_list_model_append_record:
 * @custom_list: a #MooseListModel
 * @name: Name of the person.
 * @year_born: Birthyear of the person.
 *
 * Makes wonders happen.
 *
 * Return value: nothing.
 */
void moose_list_model_append_record(
    MooseListModel * custom_list,
    const gchar * name,
    guint year_born
);

G_END_DECLS

#endif /* MOOSE_LIST_MODEL_H */
