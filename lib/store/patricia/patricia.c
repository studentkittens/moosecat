#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "patricia.h"

/* for debugging purposes this will execute all included printf statements */
//#define _DEBUG 1

/* the trivial translator */
int patricia_trivial_translator[256] = {
    1,   2,   3,   4,   5,   6,   7,   8, 
    9,  10,  11,  12,  13,  14,  15,  16, 
    17,  18,  19,  20,  21,  22,  23,  24, 
    25,  26,  27,  28,  29,  30,  31,  32, 
    33,  34,  35,  36,  37,  38,  39,  40, 
    41,  42,  43,  44,  45,  46,  47,  48, 
    49,  50,  51,  52,  53,  54,  55,  56, 
    57,  58,  59,  60,  61,  62,  63,  64, 
    65,  66,  67,  68,  69,  70,  71,  72, 
    73,  74,  75,  76,  77,  78,  79,  80, 
    81,  82,  83,  84,  85,  86,  87,  88, 
    89,  90,  91,  92,  93,  94,  95,  96, 
    97,  98,  99, 100, 101, 102, 103, 104, 
    105, 106, 107, 108, 109, 110, 111, 112, 
    113, 114, 115, 116, 117, 118, 119, 120, 
    121, 122, 123, 124, 125, 126, 127, 128, 
    129, 130, 131, 132, 133, 134, 135, 136, 
    137, 138, 139, 140, 141, 142, 143, 144, 
    145, 146, 147, 148, 149, 150, 151, 152, 
    153, 154, 155, 156, 157, 158, 159, 160, 
    161, 162, 163, 164, 165, 166, 167, 168, 
    169, 170, 171, 172, 173, 174, 175, 176, 
    177, 178, 179, 180, 181, 182, 183, 184, 
    185, 186, 187, 188, 189, 190, 191, 192, 
    193, 194, 195, 196, 197, 198, 199, 200, 
    201, 202, 203, 204, 205, 206, 207, 208, 
    209, 210, 211, 212, 213, 214, 215, 216, 
    217, 218, 219, 220, 221, 222, 223, 224, 
    225, 226, 227, 228, 229, 230, 231, 232, 
    233, 234, 235, 236, 237, 238, 239, 240, 
    241, 242, 243, 244, 245, 246, 247, 248, 
    249, 250, 251, 252, 253, 254, 255, 256 };


/* translator for printable characters */
int patricia_print_translator[256] = {
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    1,   2,   3,   4,   5,   6,   7,   8, 
    9,  10,  11,  12,  13,  14,  15,  16, 
    17,  18,  19,  20,  21,  22,  23,  24, 
    25,  26,  27,  28,  29,  30,  31,  32, 
    33,  34,  35,  36,  37,  38,  39,  40, 
    41,  42,  43,  44,  45,  46,  47,  48, 
    49,  50,  51,  52,  53,  54,  55,  56, 
    57,  58,  59,  60,  61,  62,  63,  64, 
    65,  66,  67,  68,  69,  70,  71,  72, 
    73,  74,  75,  76,  77,  78,  79,  80, 
    81,  82,  83,  84,  85,  86,  87,  88, 
    89,  90,  91,  92,  93,  94,  95,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0 };

/* translator for alphanumeric characters */
int patricia_alnum_translator[256] = {
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    1,   2,   3,   4,   5,   6,   7,   8, 
    9,  10,   0,   0,   0,   0,   0,   0, 
    0,  11,  12,  13,  14,  15,  16,  17, 
    18,  19,  20,  21,  22,  23,  24,  25, 
    26,  27,  28,  29,  30,  31,  32,  33, 
    34,  35,  36,   0,   0,   0,   0,   0, 
    0,  37,  38,  39,  40,  41,  42,  43, 
    44,  45,  46,  47,  48,  49,  50,  51, 
    52,  53,  54,  55,  56,  57,  58,  59, 
    60,  61,  62,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0 };


/* translator for alphabetic characters */
int patricia_alpha_translator[256] = {
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   1,   2,   3,   4,   5,   6,   7, 
    8,   9,  10,  11,  12,  13,  14,  15, 
    16,  17,  18,  19,  20,  21,  22,  23, 
    24,  25,  26,   0,   0,   0,   0,   0, 
    0,  27,  28,  29,  30,  31,  32,  33, 
    34,  35,  36,  37,  38,  39,  40,  41, 
    42,  43,  44,  45,  46,  47,  48,  49, 
    50,  51,  52,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0, 
    0,   0,   0,   0,   0,   0,   0,   0 };

/* translator that does not distinguish between uppercase and lowercase */
int patricia_nocase_translator[256] = {
    0,   1,   2,   3,   4,   5,   6,   7, 
    8,   9,  10,  11,  12,  13,  14,  15, 
    16,  17,  18,  19,  20,  21,  22,  23, 
    24,  25,  26,  27,  28,  29,  30,  31, 
    32,  33,  34,  35,  36,  37,  38,  39, 
    40,  41,  42,  43,  44,  45,  46,  47, 
    48,  49,  50,  51,  52,  53,  54,  55, 
    56,  57,  58,  59,  60,  61,  62,  63, 
    64,  97,  98,  99, 100, 101, 102, 103, 
    104, 105, 106, 107, 108, 109, 110, 111, 
    112, 113, 114, 115, 116, 117, 118, 119, 
    120, 121, 122,  91,  92,  93,  94,  95, 
    96,  97,  98,  99, 100, 101, 102, 103, 
    104, 105, 106, 107, 108, 109, 110, 111, 
    112, 113, 114, 115, 116, 117, 118, 119, 
    120, 121, 122, 123, 124, 125, 126, 127, 
    128, 129, 130, 131, 132, 133, 134, 135, 
    136, 137, 138, 139, 140, 141, 142, 143, 
    144, 145, 146, 147, 148, 149, 150, 151, 
    152, 153, 154, 155, 156, 157, 158, 159, 
    160, 161, 162, 163, 164, 165, 166, 167, 
    168, 169, 170, 171, 172, 173, 174, 175, 
    176, 177, 178, 179, 180, 181, 182, 183, 
    184, 185, 186, 187, 188, 189, 190, 191, 
    192, 193, 194, 195, 196, 197, 198, 199, 
    200, 201, 202, 203, 204, 205, 206, 207, 
    208, 209, 210, 211, 212, 213, 214, 215, 
    216, 217, 218, 219, 220, 221, 222, 223, 
    224, 225, 226, 227, 228, 229, 230, 231, 
    232, 233, 234, 235, 236, 237, 238, 239, 
    240, 241, 242, 243, 244, 245, 246, 247, 
    248, 249, 250, 251, 252, 253, 254, 255 };


/**
 * Allocate a node in a patricia tree
 */
static patricia_node *allocate_node(int degree) {
    patricia_node *n;

    if ((n = malloc(sizeof(patricia_node))) == NULL) {
        fprintf(stderr, "patricia:allocate_node(1) : Out of memory\n");
        exit(-1);
    }

    memset(n, '\0', sizeof(patricia_node));

    if ((n->edges = malloc(degree*sizeof(patricia_edge))) == NULL) {
        fprintf(stderr, "patricia:allocate_node(2) : Out of memory\n");
        exit(-1);
    }

    memset(n->edges, '\0', degree*sizeof(patricia_edge));

    return n;
}

/**
 * Create a new (empty) patricia tree.  The translator is an array of
 * 256 ints that maps each ASCII character onto an integer.
 */
int patricia_create(patricia_tree *pt, int *translator) {
    int i;

    memset(pt, '\0', sizeof(patricia_tree));

    /* set the translator */
    pt->translator = translator;

    /* Compute the maximum degree of a node */
    pt->degree = 0;

    for (i = 0; i < 255; ++i) {
        if (translator[i] > pt->degree) {
            pt->degree = translator[i];
        }
    }

    pt->degree += 1;

    /* Allocate the root node */
    pt->root = allocate_node(pt->degree);

    return PATRICIA_SUCCESS;
}


/** 
 * Delete the nodes of patricia tree.
 */
static void recursive_cleanup(patricia_tree *pt, patricia_node *n) {
    int i;

    for (i = 0; i < pt->degree; ++i) {
        if (n->edges[i].child != NULL) {
            recursive_cleanup(pt, n->edges[i].child);
        }
    }

    free(n->edges);
    free(n);
}

/**
 * Cleanup a patricia tree
 */
int patricia_cleanup(patricia_tree *pt) {
    recursive_cleanup(pt, pt->root);

    memset(pt, '\0', sizeof(patricia_tree));

    return PATRICIA_SUCCESS;
}

/**
 * Insert the string s into the patricia tree pt
 */
int patricia_insert(patricia_tree *pt, unsigned char *s, void *data) {   
    int i, j, child, split;
    patricia_node *n, *u, *v;

#ifdef _DEBUG
    printf("Inserting: %s\n", s);
#endif

    n = pt->root;

    for (i = 0; pt->translator[s[i]] != PATRICIA_EOS; ++i) {
        child = pt->translator[s[i]];

#ifdef _DEBUG
        printf("Edge being followed is: %d\n", child);
#endif

        if (n->edges[child].child == NULL) {
            n->edges[child].child = allocate_node(pt->degree);
            n->edges[child].label.first = s + i;

            while (pt->translator[s[i]] != PATRICIA_EOS) {
                ++i;
            }

            n->edges[child].label.last = s + i;
            n->edges[child].child->data = data;

#ifdef _DEBUG
            printf("Edge from n to n->edges[child] contains: %c .. %c\n", n->edges[child].label.first[0], n->edges[child].label.last[0]);
#endif

            return PATRICIA_SUCCESS;
        } else {
            j = 0;

#ifdef _DEBUG
            printf("Matching characters along edge %d\n", child);
#endif
            /* match characters along edge with characters in string being inserted until they are no longer the same */
            while ((pt->translator[n->edges[child].label.first[j]] != PATRICIA_EOS) &&
                    (pt->translator[n->edges[child].label.first[j]] == pt->translator[s[i]]) &&
                    ((n->edges[child].label.first + (j-1)) != n->edges[child].label.last)) {
#ifdef _DEBUG
                printf("Edge: %c, String: %c\n", n->edges[child].label.first[j], s[i]);
#endif
                ++i;
                ++j;
            }

            if ((n->edges[child].label.first + (j-1)) == n->edges[child].label.last) {
#ifdef _DEBUG
                printf("Reached the end of string on edge from n to n->edges[child]\n");
#endif
                /* set n to n->edges[child].child, decrement i (to make up for looping increment)  and continue looping */
                n = n->edges[child].child;
            } else if ((pt->translator[n->edges[child].label.first[j]] == PATRICIA_EOS) && (pt->translator[s[i]] == PATRICIA_EOS)) {
#ifdef _DEBUG
                printf("String already exists in tree.\n");
#endif          
                return PATRICIA_ELEMENTALREADYTHERE;
            } else {
#ifdef _DEBUG
                printf("Splitting on: %c\n", n->edges[child].label.first[j]);
#endif

                /* allocate new node for remainder of characters along current edge */
                u = allocate_node(pt->degree);
                /* save old child as it will need to become a child of newly inserted node */
                v = n->edges[child].child;

                /* setup edge from u to v and set v as child of u */      
                split = pt->translator[n->edges[child].label.first[j]];

                u->edges[split].child = v;
                u->edges[split].label.first = n->edges[child].label.first + j;
                u->edges[split].label.last = n->edges[child].label.last;

#ifdef _DEBUG
                printf("Edge from u to v contains: %c .. %c\n", u->edges[split].label.first[0], u->edges[split].label.last[0]);
#endif      

                /* update last pointer of edge from n to u to be last common letter between string assoc with edge and s */
                n->edges[child].label.last = n->edges[child].label.first + (j - 1);

#ifdef _DEBUG
                printf("Edge from n to u contains: %c .. %c\n", n->edges[child].label.first[0], n->edges[child].label.last[0]);
#endif      

                /* set u as child of n */
                n->edges[child].child = u;

                /* set n to v */
                n = u;
            }

            /* need to check if we have reached the end of s; if so we need to insert a new child node with '\0' edge since for loop will just quit */
            if (pt->translator[s[i]] == PATRICIA_EOS) {
#ifdef _DEBUG
                printf("End of s reached; append node along edge of PATRICIA_EOS\n");
#endif          
                n->edges[PATRICIA_EOS].child = allocate_node(pt->degree);
                n->edges[PATRICIA_EOS].label.first = s + i;
                n->edges[PATRICIA_EOS].label.last = s+i;
                n->edges[PATRICIA_EOS].child->data = data;

#ifdef _DEBUG
                printf("Edge from n to n->edges[PATRICIA_EOS] contains: %c .. %c\n", n->edges[PATRICIA_EOS].label.first[0], n->edges[PATRICIA_EOS].label.last[0]);
#endif        
            }

            /* decrement i in order to make up for additional increment in for loop */
            --i;
        }
    }

    return PATRICIA_SUCCESS;  /* ? */
}


/* Delete the string s from the patricia tree pt
*/
int patricia_delete(patricia_tree *pt, unsigned char *s) {
    int i, j, k, child, sibling, prev, found, len, count;
    patricia_node *n, *u, *x;

#ifdef _DEBUG
    printf("Deleting: %s\n", s);
#endif  

    n = pt->root;
    u = NULL;
    found = -1;

    for (i = 0; i < pt->translator[s[i]] != PATRICIA_EOS; ++i) {
        child = pt->translator[s[i]];

#ifdef _DEBUG
        printf("Edge being followed is: %d\n", child);
#endif

        if (n->edges[child].child == NULL) {
            /* can't go any further in tree thus s mustn't exist */  
#ifdef _DEBUG
            printf("Reached leaf and still haven't found s.\n");
#endif

            return PATRICIA_ELEMENTMISSING;
        } else {
            j = 0;

#ifdef _DEBUG
            printf("Matching characters along edge %d\n", child);
#endif
            /* match characters along edge with characters in string being inserted until they are no longer the same */
            while ((pt->translator[n->edges[child].label.first[j]] != PATRICIA_EOS) &&
                    (pt->translator[n->edges[child].label.first[j]] == pt->translator[s[i]]) &&
                    ((n->edges[child].label.first + (j-1)) != n->edges[child].label.last)) {
#ifdef _DEBUG
                printf("Edge: %c, String: %c\n", n->edges[child].label.first[j], s[i]);
#endif
                ++i;
                ++j;
            }

            if ((n->edges[child].label.first + (j-1)) == n->edges[child].label.last) {
                /* we have finished with the current edge and still haven't found s so go onto the child of n */
#ifdef _DEBUG
                printf("Reached the end of string on edge from n to n->edges[child]\n");
#endif
                /* set n to n->edges[child].child and let prev be child since we need to keep track of parent edge of n */
                u = n;
                prev = child;
                n = n->edges[child].child;
            } else if ((pt->translator[n->edges[child].label.first[j]] == PATRICIA_EOS) && (pt->translator[s[i]] == PATRICIA_EOS)) {
                /* we have found s in the patricia tree... let's get outta here! */
#ifdef _DEBUG
                printf("String exists in tree.\n");
#endif
                /* we have found s and n is the parent node of the node to be removed */
                found = 1;

                break;
            } else {
                /* we have matched as much as we can on this edge and yet there still remain chars in s so it mustn't be in the tree */
#ifdef _DEBUG
                printf("String %s doesn't exist in tree\n", s);
#endif

                return PATRICIA_ELEMENTMISSING;
            }

            /* need to check if we have reached the end of s; if so check to see if the edge at '\0' exists in the child... if so, s exists in the tree */
            if (pt->translator[s[i]] == PATRICIA_EOS) {
#ifdef _DEBUG
                printf("End of s reached\n");
#endif
                if (n->edges[PATRICIA_EOS].child != NULL) {
#ifdef _DEBUG
                    printf("Found edge with PATRICIA_EOS, thus s exists\n");
#endif            
                    found = 1;
                    child = PATRICIA_EOS;

                    /* we'll need this later! */
                    ++i;

                    break;
                } else {
#ifdef _DEBUG
                    printf("Didn't find edge with PATRICIA_EOS, thus s doesn't exist\n");
#endif            
                    return PATRICIA_ELEMENTMISSING;
                }
            }

            /* decrement i in order to make up for additional increment in for loop */
            --i;
        }
    }

    if (found == 1) {
#ifdef _DEBUG
        printf("Found string %s along edge %d\n", s, child);
#endif    

        /* we found the string... now time to delete it! */
        x = n->edges[child].child;
        n->edges[child].child = NULL;

        /* check to see if there are more than 1 child nodes of n, if not, merge remaining node into parent */
        count = 0;

        for (k = 0; k < pt->degree; ++k) {
            if (n->edges[k].child != NULL) {
                ++count;
                sibling = k;
            }
        }

#ifdef _DEBUG
        printf("Number of children after deleting node: %d\n", count);
#endif
        /* only one child left so we need to merge edge leading to remainding child into parent edge */
        if (count == 1) {
            if (u != NULL) {
                len = 0;

                while (u->edges[prev].label.first + (len-1) != u->edges[prev].label.last) {
                    ++len;
                }

                /* need to account for last letter */
                ++len;

#ifdef _DEBUG
                printf("Length of parent edge: %d\n", len);
#endif        

#ifdef _DEBUG
                printf("Merging last child into parent.\n");
                printf("Edge from u to n used to be: %c .. %c\n", u->edges[prev].label.first[0], u->edges[prev].label.last[0]);
#endif
                /* update first and last pointers of parent edge to make sure that they point to the same string */
                u->edges[prev].label.first = n->edges[sibling].label.first - len + 1;
                u->edges[prev].label.last = n->edges[sibling].label.last;

#ifdef _DEBUG
                printf("Edge from u to n is now: %c .. %c\n", u->edges[prev].label.first[0], u->edges[prev].label.last[0]);
#endif

                /* set child of u->edges[prev] to be child of n->edges[child] */
                u->edges[prev].child = n->edges[sibling].child;
            }
        }

        return PATRICIA_SUCCESS;
    }

    /* only way we can get here is if the string doesn't exist */
    return PATRICIA_ELEMENTMISSING;
}

/* Search for the string s in the patricia tree
 * pt.  Fill in the data pointer to be the data specified when s was
 * inserted.
 */
int patricia_search(patricia_tree *pt, unsigned char *s, void **data)
{
    int i, j, child;
    patricia_node *n;

#ifdef _DEBUG
    printf("Searching for: %s\n", s);
#endif

    n = pt->root;

    for (i = 0; i < pt->translator[s[i]] != PATRICIA_EOS; ++i) {
        child = pt->translator[s[i]];

#ifdef _DEBUG
        printf("Edge being followed is: %d\n", child);
#endif

        if (n->edges[child].child == NULL) {
            /* can't go any further in tree thus s mustn't exist */  
#ifdef _DEBUG
            printf("Reached leaf and still haven't found s.\n");
#endif

            return PATRICIA_ELEMENTMISSING;
        } else {
            j = 0;

#ifdef _DEBUG
            printf("Matching characters along edge %d\n", child);
#endif
            /* match characters along edge with characters in string being inserted until they are no longer the same */
            while ((pt->translator[n->edges[child].label.first[j]] != PATRICIA_EOS) &&
                    (pt->translator[n->edges[child].label.first[j]] == pt->translator[s[i]]) &&
                    ((n->edges[child].label.first + (j-1)) != n->edges[child].label.last)) {
#ifdef _DEBUG
                printf("Edge: %c, String: %c\n", n->edges[child].label.first[j], s[i]);
#endif
                ++i;
                ++j;
            }

            if ((n->edges[child].label.first + (j-1)) == n->edges[child].label.last) {
                /* we have finished with the current edge and still haven't found s so go onto the child of n */
#ifdef _DEBUG
                printf("Reached the end of string on edge from n to n->edges[child]\n");
#endif
                /* set n to n->edges[child].child */
                n = n->edges[child].child;
            } else if ((pt->translator[n->edges[child].label.first[j]] == PATRICIA_EOS) && (pt->translator[s[i]] == PATRICIA_EOS)) {
                /* we have found s in the patricia tree... let's get outta here! */
#ifdef _DEBUG
                printf("String exists in tree.\n");
#endif
                /* set data */
                data = &(n->edges[child].child->data);

                return PATRICIA_SUCCESS;
            } else {
                /* we have matched as much as we can on this edge and yet there still remain chars in s so it mustn't be in the tree */
#ifdef _DEBUG
                printf("String %s doesn't exist in tree\n", s);
#endif

                return PATRICIA_ELEMENTMISSING;
            }

            /* need to check if we have reached the end of s; if so check to see if the edge at '\0' exists in the child... if so, s exists in the tree */
            if (pt->translator[s[i]] == PATRICIA_EOS) {
#ifdef _DEBUG
                printf("End of s reached\n");
#endif
                if (n->edges[PATRICIA_EOS].child != NULL) {
#ifdef _DEBUG
                    printf("Found edge with PATRICIA_EOS, thus s exists\n");
#endif
                    /* set data */
                    data = &(n->edges[PATRICIA_EOS].child->data);

                    return PATRICIA_SUCCESS;
                } else {
#ifdef _DEBUG
                    printf("Didn't find edge with PATRICIA_EOS, thus s doesn't exist\n");
#endif            
                    return PATRICIA_ELEMENTMISSING;
                }
            }

            /* decrement i in order to make up for additional increment in for loop */
            --i;
        }
    }

    /* only way we can get here is if the string doesn't exist */
    return PATRICIA_ELEMENTMISSING;
}

/* Return true if the patricia tree contains some string whose prefix is s
*/
patricia_node *patricia_hasprefix(patricia_tree *pt, unsigned char *s)
{
    int i, j, child;
    patricia_node *n = NULL;

#ifdef _DEBUG
    printf("Searching for prefix: %s\n", s);
#endif

    n = pt->root;

    for (i = 0; i < pt->translator[s[i]] != PATRICIA_EOS; ++i) {
        child = pt->translator[s[i]];

#ifdef _DEBUG
        printf("Edge being followed is: %d\n", child);
#endif

        if (n->edges[child].child == NULL) {
            /* can't go any further in tree thus s mustn't exist */  
#ifdef _DEBUG
            printf("Reached leaf and still haven't found s.\n");
#endif
            return NULL;
        } else {
            j = 0;

#ifdef _DEBUG
            printf("Matching characters along edge %d\n", child);
#endif
            /* match characters along edge with characters in string being inserted until they are no longer the same */
            while ((pt->translator[n->edges[child].label.first[j]] != PATRICIA_EOS) &&
                    (pt->translator[n->edges[child].label.first[j]] == pt->translator[s[i]]) &&
                    ((n->edges[child].label.first + (j-1)) != n->edges[child].label.last)) {
#ifdef _DEBUG
                printf("Edge: %c, String: %c\n", n->edges[child].label.first[j], s[i]);
#endif
                ++i;
                ++j;
            }

            if ((n->edges[child].label.first + (j-1)) == n->edges[child].label.last) {
                /* we have finished with the current edge and still haven't found s so go onto the child of n */
#ifdef _DEBUG
                printf("Reached the end of string on edge from n to n->edges[child]\n");
#endif
                /* set n to n->edges[child].child */
                n = n->edges[child].child;
            } else if ((pt->translator[n->edges[child].label.first[j]] == PATRICIA_EOS) && (pt->translator[s[i]] == PATRICIA_EOS)) {
                /* we have found s in the patricia tree... let's get outta here! */
#ifdef _DEBUG
                printf("String exists in tree.\n");
#endif 

                return n;
            } else {
                if (pt->translator[s[i]] == PATRICIA_EOS) {
                    /* we have found the prefix s in the tree */
#ifdef _DEBUG
                    printf("Prefix %s found in tree\n", s);
#endif

                    return n;
                } else {            
                    /* we have matched as much as we can on this edge and yet there still remain chars in s so it mustn't be in the tree */
#ifdef _DEBUG
                    printf("Prefix %s doesn't exist in tree\n", s);
#endif

                    return NULL;
                }
            }

            /* decrement i in order to make up for additional increment in for loop */
            --i;
        }
    }

    /* if we have gotten this far we must have found the prefix */
    return n;
}

/* Recursive enumerate */
static int recursive_patricia_enumerate(patricia_tree *pt, patricia_node *n, int len, patricia_report_func f)
{
    int i, j, newlen;

    for (i = 0; i < pt->degree; ++i) {
        if (n->edges[i].child != NULL) {
            j=0;
            newlen = len;

            while (n->edges[i].label.first + (j-1) != n->edges[i].label.last) {
                ++newlen;
                ++j;
            }

#ifdef _DEBUG
            printf("Current length of string: %d\n", newlen);
#endif
            if (pt->translator[n->edges[i].label.last[0]] == PATRICIA_EOS) {
#ifdef _DEBUG
                printf("Reached end of a string\n");
#endif              
                /* we have reached the end of this string so print the string and return */
                f(n->edges[i].label.last - newlen + 1, NULL);

                continue;
            }

            recursive_patricia_enumerate(pt, n->edges[i].child, newlen, f);
        }
    }

    return PATRICIA_SUCCESS;
}

/** 
 * Enumerate the strings stored in the patricia tree pt
 */
int patricia_enumerate(patricia_tree *pt, patricia_report_func f)
{
    int i, len;
    patricia_node *n = pt->root;

    for (i = 0; i < pt->degree; ++i) {
        if (n->edges[i].child != NULL) {
            len = 0;

            while (n->edges[i].label.first + (len-1) != n->edges[i].label.last) { 
                ++len;
            }

#ifdef _DEBUG
            printf("Current length of string: %d\n", len);
#endif
            /* check to see if this is only string extending from this part of the root */
            if (pt->translator[n->edges[i].label.last[0]] == PATRICIA_EOS) {
                f(n->edges[i].label.first, NULL);
                continue;
            }

            /* we have to dig deeper so go into the recursive enumerate */    
            recursive_patricia_enumerate(pt, n->edges[i].child, len, f);
        }
    }

    return PATRICIA_SUCCESS;
}
