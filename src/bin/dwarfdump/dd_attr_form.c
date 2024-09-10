/*
  Copyright (C) 2021 David Anderson. All Rights Reserved.

  This program is free software; you can redistribute it and/or
  modify it under the terms of version 2 of the GNU General
  Public License as published by the Free Software Foundation.

  This program is distributed in the hope that it would be
  useful, but WITHOUT ANY WARRANTY; without even the implied
  warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.

  Further, this software is distributed without any warranty
  that it is free of the rightful claim of any third person
  regarding infringement or the like.  Any license provided
  herein, whether implied or otherwise, applies only to this
  software file.  Patent licenses, if any, provided herein
  do not apply to combinations of this program with other
  software, or any other product whatsoever.

  You should have received a copy of the GNU General Public
  License along with this program; if not, write the Free
  Software Foundation, Inc., 51 Franklin Street - Fifth Floor,
  Boston MA 02110-1301, USA.
*/

#include <config.h>

#include <stdio.h>  /* printf() */
#include <stdlib.h> /* calloc() free() malloc() */

/* Windows specific header files */
#if defined(_WIN32) && defined(HAVE_STDAFX_H)
#include "stdafx.h"
#endif /* HAVE_STDAFX_H */

#include "dwarf.h"
#include "libdwarf.h"
#include "libdwarf_private.h"
#include "dd_defined_types.h"
#include "dd_checkutil.h"
#include "dd_glflags.h"
#include "dd_globals.h"
#include "dd_tsearchbal.h"
#include "dd_naming.h"
#include "dd_attr_form.h"
#include "dwarfdump-af-table.h"
#include "dwarfdump-ta-table.h"
#include "dwarfdump-ta-ext-table.h"
#include "dwarfdump-tt-table.h"
#include "dwarfdump-tt-ext-table.h"

/*  Here we have  code to read the generated header files
    with the relationship data so we can print the data.
    This also prints the attr-form tables.
    See also print_tag_attributes_usage.c as that is
    where tag-tag and tag-attr tree is printed. */ 


#if 0
static void
print_3key_record(int num,Three_Key_Entry *e)
{
    printf("3key %d 0x%x 0x%x 0x%x st %d%d ct %lu\n",
        num,e->key1,e->key2,e->key3,
        e->std_or_exten,e->from_tables,
        (unsigned long)e->count);
}
#endif /* 0 */

void * threekey_tag_tag_base; /* tag-tree recording */
void * threekey_tag_attr_base; /* for tag_attr recording */
void * threekey_attr_form_base; /* for attr/class/form recording */
void * threekey_tag_use_base; /* for simple tag counting */

int
make_3key(Dwarf_Half k1,
    Dwarf_Half k2,
    Dwarf_Half k3,
    Dwarf_Small std_or_exten,
    Dwarf_Small reserved,
    Dwarf_Unsigned count,
    Three_Key_Entry ** out)
{
    Three_Key_Entry *e =
        (Three_Key_Entry *)malloc(sizeof(Three_Key_Entry));
    if (!e) {
        return DW_DLV_ERROR; /* out of memory */
    }
    e->key1         = k1;
    e->key2         = k2;
    e->key3         = k3;
    e->from_tables  = std_or_exten; 
    e->reserved     = reserved;
    e->count        = count;
    *out            = e;
    return DW_DLV_OK;
}

void
free_func_3key_entry(void *keystructptr)
{
    Three_Key_Entry *e = keystructptr;
    free(e);
}

int
std_compare_3key_entry(const void *l_in, const void *r_in)
{
    const Three_Key_Entry *l = l_in;
    const Three_Key_Entry *r = r_in;
    if (l->key1 < r->key1) {
        return -1;
    }
    if (l->key1 > r->key1) {
        return 1;
    }
    if (l->key2 < r->key2) {
        return -1;
    }
    if (l->key2 > r->key2) {
        return 1;
    }
    if (l->key3 < r->key3) {
        return -1;
    }
    if (l->key3 > r->key3) {
        return 1;
    }
    return 0;

}

static Dwarf_Unsigned counting_global;
static void
count_3key_entry(const void * vptr,
    DW_VISIT x,
    int level)
{
    (void)vptr;
    (void)level;
    if (x == dwarf_preorder || x == dwarf_leaf) {
        ++counting_global;
    }
}

/*  Tree argument expected is threekey_attr_form_base for example */
Dwarf_Unsigned
three_key_entry_count(void *base)
{
    Dwarf_Unsigned count = 0;

    counting_global = 0;
    dwarf_twalk(base,count_3key_entry);
    count = counting_global;
    counting_global = 0;
    return count;
}

/*  Used for base table creation */
static int
dd_insert_table_entry(void *tree,
    Three_Key_Entry *e,
    int *errnum)
{
    Three_Key_Entry *re = 0;
    void *ret = dwarf_tsearch(e,tree,std_compare_3key_entry);
    if (!ret) {
        *errnum = DW_DLE_ALLOC_FAIL;
        return DW_DLV_ERROR;
    }
    re = *(Three_Key_Entry **)ret;
    if (re == e) {
#if 0
printf("dadebug added  0x%x 0x%x 0x%x 0x%x line %d\n",
e->key1,e->key2,e->key3,e->from_tables,__LINE__);
#endif
        /* Normal. Added. */
        return DW_DLV_OK;
    }
printf("dadebug duplicate of e 0x%x 0x%x 0x%x  0x%x line %d\n",
e->key1,e->key2,e->key3,e->from_tables,__LINE__);
printf("dadebug duplicate was re 0x%x 0x%x 0x%x 0x%x line %d\n",
re->key1,re->key2,re->key3,re->from_tables,__LINE__);
    /*  A full duplicate in the table. Oops.
        Not a great choice of error code. */
    *errnum = DW_DLE_ATTR_FORM_BAD;
    return DW_DLV_ERROR;
}

/*  tree argument expected is
    &threekey_attr_form_base for example 
    Somenthing similar for all the tag_tag tag_attr trees */
static int
insert_new_af_tab_entry(void *tree,
    struct af_table_s * tab,
    int *errnum)
{
    Three_Key_Entry *e = 0;
    int res = 0;

    res = make_3key(tab->attr,tab->formclass,0,
        tab->section,
        0 /* reserved */,
        0 /* count is zero during preset   */,
        &e);
    if (res != DW_DLV_OK) {
        *errnum = DW_DLE_ALLOC_FAIL;
        return res;
    }
    return dd_insert_table_entry(tree,e,errnum);
}

/*  This is for dwarfdump to call at runtime.
    Returns DW_DLV_OK on success  */
/* something similar for the tag_tag tag_attr tables. */
int
dd_build_attr_form_base_tree(int*errnum)
{
    struct af_table_s * tab = 0;
    int res;
    void *tree = &threekey_attr_form_base;

    if (threekey_attr_form_base) {
        /*  Do not init again if a tied file */
        return DW_DLV_OK;
    }
    /*  section here is a misnomer, it means  AF_STD
        or AF_EXTEN or AF_UNKNOWN  */
    for (tab = &attr_formclass_table[0]; ; tab++) {
        if ((!tab->attr) &&
            (!tab->formclass) &&
            (!tab->section)) {
            /* Done */
printf("dadebug End of af_table reading line %d\n",__LINE__);
            break;
        }
        res  = insert_new_af_tab_entry(tree,tab,errnum);
        if (res != DW_DLV_OK) {
            return res;
        }
    }
    return DW_DLV_OK;
}

/*  For simple counting of tag uses,
    there is nothing we need to do here. 
    Adding a record via record_tag_usage()
    will initialize. */
int
dd_build_tag_use_base_tree( int*errnum)
{
    (void)errnum;
    return DW_DLV_OK;
}

int
dd_build_tag_attr_base_tree( int*errnum)
{
    unsigned int i = 0;
    unsigned int k = 0;
    unsigned int t3 = 0;
    int res = 0;
    unsigned reserved = 0;
    unsigned initial_count = 0;
    void * tree = &threekey_tag_attr_base;
     
printf("dadebug tag_attr start line %d\n",__LINE__);
    for (i=0 ; i < ATTR_TREE_EXT_COLUMN_COUNT; ++i) {
        unsigned t1 = tag_attr_combination_ext_table[i][0]; 
        for (k=1 ; k < ATTR_TREE_EXT_COLUMN_COUNT; ++k) { 
            unsigned t2 = tag_attr_combination_ext_table[i][k];
            if (t1 && t2) {
                Three_Key_Entry *e = 0;
                res = make_3key(t1,t2,t3,AF_EXTEN,
                   reserved,initial_count,
                   &e);
                if (res != DW_DLV_OK) {
                   *errnum = DW_DLE_ALLOC_FAIL;
printf("dadebug fail line %d\n",__LINE__);
                   return res;
                }
                res =  dd_insert_table_entry(tree,e,errnum);
                if (res != DW_DLV_OK) {
                   *errnum = DW_DLE_ALLOC_FAIL;
printf("dadebug fail line %d\n",__LINE__);
                   return res;
                }
            }
        }
    }
    for (i=0 ; i < ATTR_TREE_COLUMN_COUNT; ++i) {
        unsigned t1 = tag_attr_combination_table[i][0];
        for (k=1 ; k < ATTR_TREE_COLUMN_COUNT; ++k) {
            unsigned t2 = tag_attr_combination_table[i][k];
            if (t1 && t2) {
                Three_Key_Entry *e = 0;
                res = make_3key(t1,t2,t3,AF_STD,
                   reserved,initial_count,
                   &e); 
                if (res != DW_DLV_OK) {
                   *errnum = DW_DLE_ALLOC_FAIL;
printf("dadebug fail line %d\n",__LINE__);
                   return res;
                }
                res =  dd_insert_table_entry(tree,e,errnum);
                if (res != DW_DLV_OK) {
printf("dadebug fail line %d\n",__LINE__);
                   return res;
                }
            }
        }
    }
    return DW_DLV_OK;
}

int
dd_build_tag_tag_base_tree( int*errnum)
{
    unsigned int i = 0;
    unsigned int k = 0;
    unsigned int t3 = 0;
    int res = 0;
    unsigned reserved = 0;
    unsigned initial_count = 0;
    void * tree = &threekey_tag_tag_base;
     
printf("dadebug tag_tag start line %d\n",__LINE__);
    for (i=0 ; i < TAG_TREE_EXT_COLUMN_COUNT; ++i) {
        unsigned t1 = tag_tree_combination_ext_table[i][0]; 
        for (k=1 ; k < TAG_TREE_EXT_COLUMN_COUNT; ++k) { 
            unsigned t2 = tag_tree_combination_ext_table[i][k];
            if (t1 && t2) {
                Three_Key_Entry *e = 0;
                res = make_3key(t1,t2,t3,AF_EXTEN,
                   reserved,initial_count,
                   &e);
                if (res != DW_DLV_OK) {
                   *errnum = DW_DLE_ALLOC_FAIL;
                   return res;
                }
                res =  dd_insert_table_entry(tree,e,errnum);
                if (res != DW_DLV_OK) {
                   *errnum = DW_DLE_ALLOC_FAIL;
                   return res;
                }
            }
        }
    }
    for (i=0 ; i < TAG_TREE_COLUMN_COUNT; ++i) {
        unsigned t1 = tag_tree_combination_table[i][0];
        for (k=1 ; k < TAG_TREE_COLUMN_COUNT; ++k) {
            unsigned t2 = tag_tree_combination_table[i][k];
            if (t1 && t2) {
                Three_Key_Entry *e = 0;
                res = make_3key(t1,t2,t3,AF_STD,
                   reserved,initial_count,
                   &e); 
                if (res != DW_DLV_OK) {
                   *errnum = DW_DLE_ALLOC_FAIL;
                   return res;
                }
                res =  dd_insert_table_entry(tree,e,errnum);
                if (res != DW_DLV_OK) {
                   *errnum = DW_DLE_ALLOC_FAIL;
                   return res;
                }
            }
        }
    }
    return DW_DLV_OK;
}
int 
dd_build_tag_attr_form_base_trees(int*errnum)
{
    int res = 0;
    
    res = dd_build_attr_form_base_tree(errnum);
    if (res != DW_DLV_OK){
printf("dadebug fail line %d\n",__LINE__);
        return res;
    }
    res = dd_build_tag_attr_base_tree(errnum);
    if (res != DW_DLV_OK){
printf("dadebug fail line %d\n",__LINE__);
        return res;
    }
    res = dd_build_tag_tag_base_tree(errnum);
    if (res != DW_DLV_OK){
printf("dadebug fail line %d\n",__LINE__);
        return res;
    }
    res = dd_build_tag_use_base_tree(errnum);
    if (res != DW_DLV_OK){
printf("dadebug fail line %d\n",__LINE__);
        return res;
    }
    return DW_DLV_OK; 
}



void
dd_destroy_tag_use_base_tree(void)
{
    if (!threekey_tag_use_base) {
        return;
    }
    dwarf_tdestroy(threekey_tag_use_base,
        free_func_3key_entry);
    threekey_tag_use_base = 0;
}
static void
dd_destroy_attr_form_tree(void)
{
    if (!threekey_attr_form_base) {
        return;
    }
    dwarf_tdestroy(threekey_attr_form_base,
        free_func_3key_entry);
    threekey_attr_form_base = 0;
}
static void
dd_destroy_tag_attr_tree(void)
{
    if (!threekey_tag_attr_base) {
        return;
    }
    dwarf_tdestroy(threekey_tag_attr_base,
        free_func_3key_entry);
    threekey_tag_attr_base = 0;
}
static void 
dd_destroy_tag_tag_tree(void)
{
    if (!threekey_tag_tag_base) {
        return;
    }
    dwarf_tdestroy(threekey_tag_tag_base,
        free_func_3key_entry);
    threekey_tag_tag_base = 0;
}

void
dd_destroy_tag_attr_form_trees(void)
{
    dd_destroy_attr_form_tree();
    dd_destroy_tag_attr_tree();
    dd_destroy_tag_tag_tree();
    dd_destroy_tag_use_base_tree();
}

/*  SKIP_AF_CHECK defined means this is in scripts/ddbuild.sh
    and this checking makes no sense and will not compile. */
#ifndef SKIP_AF_CHECK
static Dwarf_Bool
legal_attr_formclass_combination(Dwarf_Half attr,
    Dwarf_Half fc)
{
    Three_Key_Entry *e = 0;
    Three_Key_Entry *re = 0;
    void *ret = 0;
    int   res = 0;

    res = make_3key(attr,fc,0,0,0,0,&e);
    if (res!= DW_DLV_OK) {
        /*  Hiding some sort of botch/malloc issue */
        return TRUE;
    }
    ret = dwarf_tfind(e,&threekey_attr_form_base,
        std_compare_3key_entry);
    if (!ret) {
        /*  Surprising combo. */
        free(e);
        return FALSE;
    }
    re = *(Three_Key_Entry **)ret;
    if (!glflags.gf_suppress_check_extensions_tables) {
        free(e);
        return TRUE;
    }
    if (re->from_tables == AF_STD) {
        free(e);
        return TRUE;
    }
    free(e);
    return FALSE;
}


static void
check_attr_formclass_combination(Dwarf_Debug dbg,
    Dwarf_Half tag,
    Dwarf_Half attrnum,
    Dwarf_Half fc,
    int pd_dwarf_names_print_on_error,
    int die_stack_indent_level)
{
    const char *tagname = "<AT invalid>";
    const char *formclassname = "<FORM_CLASS invalid>";
    DWARF_CHECK_COUNT(attr_formclass_result,1);
    if (legal_attr_formclass_combination(attrnum,fc)) {
        /* OK */
    } else {
        /* Report errors only if tag-attr check is on */
        if (glflags.gf_check_tag_attr) {
            tagname = get_AT_name(attrnum,
                pd_dwarf_names_print_on_error);
            tag_specific_globals_setup(dbg,tag,
                die_stack_indent_level);
            formclassname = get_FORM_CLASS_name(fc,
                pd_dwarf_names_print_on_error);

            DWARF_CHECK_ERROR3(attr_formclass_result,tagname,
                formclassname,
                "check the attr-formclass combination");
        } else { /* Nothing to do. */ }
    }
}
#endif /* SKIP_AF_CHECK  */

void
record_attr_form_use(
    Dwarf_Debug dbg,
    Dwarf_Half tag,
    Dwarf_Half attr,
    Dwarf_Half fclass,
    Dwarf_Half form,
    int die_stack_indent_level)
{
    /*  SKIP_AF_CHECK defined means this is in scripts/ddbuild.sh
        and checking/recording makes no sense and will not compile. */
#ifdef SKIP_AF_CHECK
    (void)dbg;
    (void)tag;
    (void)attr;
    (void)fclass;
    (void)form;
    (void)die_stack_indent_level;;
    return;
#else
    Three_Key_Entry *e =  0;
    Three_Key_Entry *re =  0;
    void *ret =  0;
    int res = 0;

/*  SKIP_AF_CHECK defined means this is in scripts/ddbuild.sh
    and this checking makes no sense and will not compile. */
    check_attr_formclass_combination(dbg,
        tag,attr,fclass,1,
        die_stack_indent_level);
    res = make_3key(attr,fclass,form,0,0,1,&e);
    if (res!= DW_DLV_OK) {
        /*  Could print something */
        return;
    }
    ret = dwarf_tsearch(e,&threekey_attr_form_base,
        std_compare_3key_entry);
    if (!ret) {
        free_func_3key_entry(e);
        /*  Could print something */
        return;
    }
    re = *(Three_Key_Entry **)ret;
    if (re == e) {
        /*  Brand new entry. Done.
            local malloc is in the tree. */
        return;
    }
    /* Was already entered.*/
    ++re->count;
    /* Clean out the local malloc */
    free_func_3key_entry(e);
    return;
#endif /* SKIP_AF_CHECK */
}

static Dwarf_Unsigned recordcount = 0;
static Dwarf_Unsigned recordmax = 0;
static Three_Key_Entry * tkarray = 0;
static void
extract_3key_entry(const void * vptr,
    DW_VISIT x,
    int level)
{
    (void)level;
    if (x == dwarf_preorder || x == dwarf_leaf) {
        Three_Key_Entry *m = *(Three_Key_Entry **)vptr;
        if (recordcount >= recordmax) {
            /* Should never happen */
            return;
        }
        tkarray[recordcount] = *m;
        ++recordcount;
    }
}

static int
qsortformclass(const void * e1in, const void * e2in)
{
    Three_Key_Entry *e1 = (Three_Key_Entry *)e1in;
    Three_Key_Entry *e2 = (Three_Key_Entry *)e2in;

    if (e1->key2 < e2->key2) {
        return -1;
    }
    if (e1->key2 > e2->key2) {
        return 1;
    }
    return 0;
}

static int
qsortform(const void * e1in, const void * e2in)
{
    Three_Key_Entry *e1 = (Three_Key_Entry *)e1in;
    Three_Key_Entry *e2 = (Three_Key_Entry *)e2in;

    if (e1->key3 < e2->key3) {
        return -1;
    }
    if (e1->key3 > e2->key3) {
        return 1;
    }
    return 0;
}

static int
qsortcountattr(const void * e1in, const void * e2in)
{
    Three_Key_Entry *e1 = (Three_Key_Entry *)e1in;
    Three_Key_Entry *e2 = (Three_Key_Entry *)e2in;

    if (e1->count < e2->count) {
        return 1;
    }
    if (e1->count > e2->count) {
        return -1;
    }
    if (e1->key1 < e2->key1) {
        return -1;
    }
    if (e1->key1 > e2->key1) {
        return 1;
    }
    if (e1->key3 < e2->key3) {
        return -1;
    }
    if (e1->key3 > e2->key3) {
        return 1;
    }
    if (e1->key2 < e2->key2) {
        return -1;
    }
    if (e1->key2 > e2->key2) {
        return 1;
    }
    return 0;
}

void
print_attr_form_usage(void)
{
    Three_Key_Entry  *tk_l  = 0;
    Dwarf_Unsigned    i     = 0;
    unsigned          curform = 0;
    Dwarf_Unsigned    formtotal = 0;
    unsigned          curattr = 0;
    Dwarf_Unsigned    attrtotal = 0;
    Dwarf_Unsigned    j     = 0;
    float             total = 0.0f;
    float             pct   = 0.0f;
    Dwarf_Bool        startnoted = FALSE;
    const char *      localformat=  NULL;
    Dwarf_Unsigned    localsum = 0;

    recordmax = three_key_entry_count(threekey_attr_form_base);
    if (!recordmax) {
        return;
    }
    tk_l = (Three_Key_Entry *)calloc(recordmax+1,
        sizeof(Three_Key_Entry));
    tkarray=tk_l;
    if (!tk_l) {
        printf("ERROR: unable to malloc attr/form array "
            " for a summary report \n");
        glflags.gf_count_major_errors++;
        return;
    }
    /* Reset the file-global! */
    recordcount = 0;
    dwarf_twalk(threekey_attr_form_base,extract_3key_entry);
    if (recordcount != recordmax) {
        printf("ERROR: unable to fill in attr/form array "
            " for a summary report, count %lu != walk %lu \n",
            (unsigned long)recordcount,
            (unsigned long)recordmax);
        glflags.gf_count_major_errors++;
        free(tk_l);
        tkarray = 0;
        return;
    }
    for (i = 0; i < recordmax; ++i) {
        Three_Key_Entry * tke = tk_l+i;

        if (!tke->key3) {
            /* Skip table building data */
            continue;
        }
        total += (float)tke->count;
    }
    qsort(tk_l,recordmax,sizeof(Three_Key_Entry),
        qsortcountattr);
    printf("\n*** ATTRIBUTES AND FORMS USAGE ***\n");
    printf("Full record count                    : %8" DW_PR_DUu "\n",
        recordmax);
    printf("Total number of objectfile attributes: %8.0f\n", total);
    printf("[]                                        "
        "                found rate\n");
    localformat="[%3u] %-30s %-20s %7" DW_PR_DUu " %.0f%%\n";
    localsum = 0;
    for (i = 0; i < recordmax; ++i) {
        Three_Key_Entry * tke = tk_l+i;

        if (!tke->key3) {
            /* Skip table building data */
            continue;
        }
        pct = ( (float)tke->count / total)*100.0f;
        printf(localformat,
            (unsigned)i,
            get_AT_name(tke->key1,1),
            get_FORM_name(tke->key3,1),
            tke->count,pct);
        localsum += tke->count;
    }
    printf(localformat, (unsigned)(recordmax),
        "Sum found:","",localsum,100.0f);

    qsort(tk_l,recordmax,sizeof(Three_Key_Entry),
        qsortformclass);
    j = 0;
    /* Re-using the following two */
    curform = 0;
    formtotal = 0;
    startnoted = FALSE;
    printf("\n*** COUNT BY FORMCLASS ***\n");
    printf("[]                                 found rate\n");
    localsum = 0;
    localformat="[%2u] %-28s %6" DW_PR_DUu " %.0f%%\n";
    for (i = 0; i < recordmax; ++i) {
        Three_Key_Entry * tke = tk_l+i;

        if (!tke->key3) {
            /* Skip table building data */
            continue;
        }
        if (!startnoted) {
            curform = tke->key2;
            formtotal = tke->count;
            startnoted = TRUE;
            continue;
        }
        if (curform != tke->key2) {
            pct = ( (float)formtotal / total)*100.0f;
            printf(localformat,
                (unsigned)j,
                get_FORM_CLASS_name(curform,1),
                formtotal,pct);
            localsum += formtotal;
            curform = tke->key2;
            formtotal = tke->count;
            ++j;
            continue;
        }
        formtotal += tke->count;
    }
    if (formtotal) {
        pct = ( (float)formtotal / total)*100.0f;
        printf(localformat,
            (unsigned)j,
            get_FORM_CLASS_name(curform,1),
            formtotal,pct);
        localsum += formtotal;
    }
    printf(localformat, (unsigned)(j+1),
        "Sum found:",localsum,100.0f);

    /* Re-using the following two */
    curform = 0;
    formtotal = 0;
    startnoted = FALSE;
    qsort(tk_l,recordmax,sizeof(Three_Key_Entry),
        qsortform);
    j = 0;
    printf("\n*** COUNT BY FORM ***\n");
    printf("[]                         found rate\n");
    localformat="[%2u] %-20s %6" DW_PR_DUu " %.0f%%\n";
    localsum = 0;
    for (i = 0; i < recordmax; ++i) {
        Three_Key_Entry * tke = tk_l+i;

        if (!tke->key3) {
            /* Skip table building data */
            continue;
        }
        if (!startnoted) {
            curform = tke->key3;
            formtotal = tke->count;
            startnoted = TRUE;
            continue;
        }
        if (curform != tke->key3) {
            pct = ( (float)formtotal / total)*100.0f;
            printf(localformat,
                (unsigned)j,
                get_FORM_name(curform,1),
                formtotal,pct);
            localsum += formtotal;
            curform = tke->key3;
            formtotal = tke->count;
            ++j;
            continue;
        }
        formtotal += tke->count;
    }
    if (formtotal) {
        pct = ( (float)formtotal / total)*100.0f;
        printf(localformat,
            (unsigned)j,
            get_FORM_name(curform,1),
            formtotal,pct);
        localsum += formtotal;
    }
    printf(localformat, (unsigned)(j+1),
        "Sum found:",localsum,100.0f);

    j = 0;
    curattr = 0;
    attrtotal = 0;
    startnoted = FALSE;
    printf("\n*** COUNT BY ATTRIBUTE ***\n");
    printf("[]                                   found rate\n");
    localsum = 0;
    localformat="[%2u] %-30s %6" DW_PR_DUu " %.0f%%\n";
    for (i = 0; i < recordmax; ++i) {
        Three_Key_Entry * tke = tk_l+i;

        if (!tke->key3) {
            /* Skip table building data */
            continue;
        }
        if (!startnoted) {
            curattr = tke->key1;
            attrtotal = tke->count;
            startnoted = TRUE;
            continue;
        }
        if (curattr != tke->key1) {
            pct = ( (float)attrtotal / total)*100.0f;
            printf(localformat,
                (unsigned)j,
                get_AT_name(curattr,1),
                attrtotal,pct);
            localsum += attrtotal;
            curattr = tke->key1;
            attrtotal = tke->count;
            ++j;
            continue;
        }
        formtotal += tke->count;
    }
    if (attrtotal) {
        pct = ( (float)attrtotal / total)*100.0f;
        printf(localformat,
            (unsigned)j,
            get_AT_name(curattr,1),
            attrtotal,pct);
        localsum += attrtotal;
    }
    printf(localformat, (unsigned)(j+1),
        "Sum found:",localsum,100.0f);
    free(tk_l);
    tkarray = 0;
}
