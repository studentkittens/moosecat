/* File: patricia.h
 * Description: An implementation of patricia trees
 * Creator: Pat Morin
 * Created: Fri Jun 22 18:46:36 EDT 2001
 *
 * Notes: A patricia tree is used to hold string
 */
#ifndef __PATRICIA_H
#define __PATRICIA_H

/* Return codes */
#define PATRICIA_SUCCESS 0
#define PATRICIA_ELEMENTMISSING -1
#define PATRICIA_ELEMENTALREADYTHERE -2
#define PATRICIA_NOTYETIMPLEMENTED -3
#define PATRICIA_EOF -4

/* a string ends with anything that translates to 0 */
#define PATRICIA_EOS 0  

/* the trivial translator */
int patricia_trivial_translator[256];

/* translator for printable characters */
int patricia_print_translator[256];

/* translator for alphanumeric characters */
int patricia_alnum_translator[256];

/* translator for alphabetic characters */
int patricia_alpha_translator[256];

/* translator that does not distinguish between uppercase and lowercase */
int patricia_nocase_translator[256];

/* A (sub) string in a patricia tree */
typedef struct {
  unsigned char *first;  /* pointer to first character in string */
  unsigned char *last;   /* pointer to last character in string */
} patricia_string;

/* An edge in a patricia tree (labelled with a string, of course) */
typedef struct {
  patricia_string label;        /* label of edge */
  struct tag_patricia_node *child;  /* pointer to child */
} patricia_edge;

/* A node in a patricia tree */
typedef struct tag_patricia_node {
  patricia_edge *edges;         /* edge leading to children */
  void *data;
} patricia_node;

typedef struct {
  int *translator;      /* for converting characters to int */
  int degree;           /* how many children does each node have */
  patricia_node *root;  /* the root node */
} patricia_tree;


/* Callback function for patricia_enumerate */
typedef void (*patricia_report_func)(unsigned char *s, void *ctx);

/* Create a new (empty) patricia tree.  The translator is an array of
 * 256 ints that maps each ASCII character onto an integer.
 */
int patricia_create(patricia_tree *pt, int *translator);

/* Cleanup a patricia tree
 */
int patricia_cleanup(patricia_tree *pt);

/* Insert the string s into the patricia tree pt
 */
int patricia_insert(patricia_tree *pt, unsigned char *s, void *data);

/* Delete the string s from the patricia tree pt
 */
int patricia_delete(patricia_tree *pt, unsigned char *s);

/* Search for the string s in the patricia tree pt.  Fill in the data
 * pointer to be the data specified when s was inserted.
 */
int patricia_search(patricia_tree *pt, unsigned char *s, void **data);

/* Return true if the patricia tree contains some string whose prefix is s
 */
patricia_node* patricia_hasprefix(patricia_tree *pt, unsigned char *s);

/* Enumerate the strings stored in the patricia tree pt
 */
int patricia_enumerate(patricia_tree *pt, patricia_report_func f);
#endif /*__PATRICIA_H*/
