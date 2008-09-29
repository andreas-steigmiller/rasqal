/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_row.c - Rasqal Query Result Row
 *
 * Copyright (C) 2003-2008, David Beckett http://www.dajobe.org/
 * Copyright (C) 2003-2005, University of Bristol, UK http://www.bristol.ac.uk/
 * 
 * This package is Free Software and part of Redland http://librdf.org/
 * 
 * It is licensed under the following three licenses as alternatives:
 *   1. GNU Lesser General Public License (LGPL) V2.1 or any newer version
 *   2. GNU General Public License (GPL) V2 or any newer version
 *   3. Apache License, V2.0 or any newer version
 * 
 * You may not use this file except in compliance with at least one of
 * the above three licenses.
 * 
 * See LICENSE.html or LICENSE.txt at the top of this package for the
 * complete terms and further detail along with the license texts for
 * the licenses in COPYING.LIB, COPYING and LICENSE-2.0.txt respectively.
 * 
 * 
 */

#ifdef HAVE_CONFIG_H
#include <rasqal_config.h>
#endif

#ifdef WIN32
#include <win32_rasqal_config.h>
#endif

#include <stdio.h>
#include <string.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdarg.h>

#include "rasqal.h"
#include "rasqal_internal.h"




static rasqal_row*
rasqal_new_row_common(int size, int order_size)
{
  rasqal_row* row;
  
  row=(rasqal_row*)RASQAL_CALLOC(rasqal_row, 1, sizeof(rasqal_row));
  if(!row)
    return NULL;

  row->usage=1;
  row->size=size;
  row->order_size=order_size;
  
  row->values=(rasqal_literal**)RASQAL_CALLOC(array, row->size,
					      sizeof(rasqal_literal*));
  if(!row->values) {
    rasqal_free_row(row);
    return NULL;
  }

  if(row->order_size > 0) {
    row->order_values=(rasqal_literal**)RASQAL_CALLOC(array,  row->order_size,
                                                      sizeof(rasqal_literal*));
    if(!row->order_values) {
      rasqal_free_row(row);
      return NULL;
    }
  }
  
  return row;
}


/**
 * rasqal_new_row:
 * @rowsource: rowsource
 *
 * INTERNAL - Create a new query result row at an offset into the result sequence.
 *
 * Return value: a new query result row or NULL on failure
 */
rasqal_row*
rasqal_new_row(rasqal_rowsource* rowsource)
{
  int size;
  int order_size;
  rasqal_row* row;
  
  rasqal_rowsource_get_sizes(rowsource, &size, &order_size);

  row=rasqal_new_row_common(size, order_size);
  if(row)
    row->rowsource=rowsource;

  return row;
}


/**
 * rasqal_new_row_for_variables:
 * @vt: variables table
 *
 * INTERNAL - Create a new query result row suitable for a variables table
 *
 * Return value: a new query result row or NULL on failure
 */
rasqal_row*
rasqal_new_row_for_variables(rasqal_variables_table* vt)
{
  int size;
  int order_size=0;
  
  size=rasqal_variables_table_get_named_variables_count(vt);

  return rasqal_new_row_common(size, order_size);
}


/**
 * rasqal_new_row_from_row:
 * @row: query result row
 * 
 * INTERNAL - Copy a query result row.
 *
 * Return value: a copy of the query result row or NULL
 */
rasqal_row*
rasqal_new_row_from_row(rasqal_row* row)
{
  row->usage++;
  return row;
}


/**
 * rasqal_new_row_from_row_deep:
 * @row: query result row
 * 
 * INTERNAL - DEEP Copy a query result row.
 *
 * Return value: a new copy of the query result row or NULL
 */
rasqal_row*
rasqal_new_row_from_row_deep(rasqal_row* row)
{
  rasqal_row* nrow;

  nrow=rasqal_new_row_common(row->size, row->order_size);
  if(!nrow)
    return NULL;

  nrow->offset = row->offset;
  
  if(row->values) {
    int i; 
    for(i=0; i < row->size; i++) {
      if(row->values[i])
        nrow->values[i]=rasqal_new_literal_from_literal(row->values[i]);
    }
  }
  if(row->order_values) {
    int i; 
    for(i=0; i < row->order_size; i++) {
      if(row->order_values[i])
        nrow->order_values[i]=rasqal_new_literal_from_literal(row->order_values[i]);
    }
  }

  return nrow;
}


/**
 * rasqal_free_row:
 * @row: query result row
 * 
 * INTERNAL - Free a query result row object.
 */
void 
rasqal_free_row(rasqal_row* row)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(row, rasqal_row);

  if(--row->usage)
    return;
  
  if(row->values) {
    int i; 
    for(i=0; i < row->size; i++) {
      if(row->values[i])
        rasqal_free_literal(row->values[i]);
    }
    RASQAL_FREE(array, row->values);
  }
  if(row->order_values) {
    int i; 
    for(i=0; i < row->order_size; i++) {
      if(row->order_values[i])
        rasqal_free_literal(row->order_values[i]);
    }
    RASQAL_FREE(array, row->order_values);
  }

  RASQAL_FREE(rasqal_row, row);
}


/**
 * rasqal_row_print:
 * @row: query result row
 * @fp: FILE* handle
 *
 * INTERNAL - Print a query result row.
 */
void 
rasqal_row_print(rasqal_row* row, FILE* fh)
{
  rasqal_rowsource* rowsource=row->rowsource;
  int i;
  
  fputs("result[", fh);
  for(i=0; i < row->size; i++) {
    /* Do not use rasqal_query_results_get_binding_name(row->results, i); 
     * as it does not work for a construct result
     */
    const unsigned char *name=NULL;
    rasqal_literal *value;

    if(rowsource) {
      rasqal_variable* v=rasqal_rowsource_get_variable_by_offset(rowsource, i);
      if(v)
        name=v->name;
    }
    
    value=row->values[i];
    if(i > 0)
      fputs(", ", fh);
    if(name)
      fprintf(fh, "%s=", name);

    if(value)
      rasqal_literal_print(value, fh);
    else
      fputs("NULL", fh);
  }

  if(row->order_size > 0) {
    fputs(" with ordering values [", fh);

    for(i=0; i < row->order_size; i++) {
      rasqal_literal *value=row->order_values[i];
      
      if(i > 0)
        fputs(", ", fh);
      if(value)
        rasqal_literal_print(value, fh);
      else
        fputs("NULL", fh);
    }
    fputs("]", fh);

  }

  fprintf(fh, " offset %d]", row->offset);
}


/**
 * rasqal_row_set_value_at:
 * @row: query result row
 * @offset: offset into row (column number)
 * @value: literal value to set
 *
 * INTERNAL - Set the value of a variable in a query result row
 */
void
rasqal_row_set_value_at(rasqal_row* row, int offset, rasqal_literal* value)
{
  row->values[offset]=value;
}


/**
 * rasqal_new_row_sequence:
 * @world: world object ot use
 * @vt: variables table to use to declare variables
 * @row_data: row data
 * @vars_count: number of variables in row
 *
 * INTERNAL - Make a sequence of #rasqal_row* objects
 * with ariables defined into the @vt table and values in the sequence
 *
 * The @row_data parameter is an array of strings forming a table of
 * width (vars_count * 2).
 * The first row is a list of variable names at offset 0.
 * The remaining rows are values where offset 0 is a literal and
 * offset 1 is a URI string.
 * The last row is indicated by offset 0 = NULL and offset 1 = NULL
 *
 * Return value: sequence of rows or NULL on failure
 */
raptor_sequence*
rasqal_new_row_sequence(rasqal_world* world,
                        rasqal_variables_table* vt,
                        const char* const row_data[],
                        int vars_count) 
{
  raptor_sequence *seq = NULL;
  int row_i;
  int column_i;
  int failed = 0;
  
#define GET_CELL(row, column, offset) \
  row_data[((((row)*vars_count)+(column))<<1)+(offset)]

  seq = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_row, (raptor_sequence_print_handler*)rasqal_row_print);
  if(!seq)
    return NULL;

  /* row 0 is variables */
  row_i = 0;

  for(column_i = 0; column_i < vars_count; column_i++) {
    const char * var_name = GET_CELL(row_i, column_i, 0);
    size_t var_name_len = strlen(var_name);
    const unsigned char* name;
    
    name = (unsigned char*)RASQAL_MALLOC(cstring, var_name_len+1);
    if(name) {
      strncpy((char*)name, var_name, var_name_len+1);
      rasqal_variables_table_add(vt, RASQAL_VARIABLE_TYPE_NORMAL, name, NULL);
    } else {
      failed = 1;
      goto tidy;
    }
  }

  for(row_i = 1;
      GET_CELL(row_i, 0, 0) || GET_CELL(row_i, 0, 1);
      row_i++) {
    rasqal_row* row;
    
    row = rasqal_new_row_for_variables(vt);
    if(!row) {
      raptor_free_sequence(seq); seq = NULL;
      goto tidy;
    }

    for(column_i = 0; column_i < vars_count; column_i++) {
      rasqal_literal* l = NULL;

      if(GET_CELL(row_i, column_i, 0)) {
        /* string literal */
        const char* str = GET_CELL(row_i, column_i, 0);
        size_t str_len = strlen(str);
        unsigned char *val;
        val = (unsigned char*)RASQAL_MALLOC(cstring, str_len+1);
        if(val) {
          strncpy((char*)val, str, str_len+1);
          l = rasqal_new_string_literal_node(world, val, NULL, NULL);
        } else 
          failed = 1;
      } else if(GET_CELL(row_i, column_i, 1)) {
        /* URI */
        const unsigned char* str;
        raptor_uri* u;
        str = (const unsigned char*)GET_CELL(row_i, column_i, 1);
        u = raptor_new_uri(str);
        if(u)
          l = rasqal_new_uri_literal(world, u);
        else
          failed = 1;
      } /* else invalid and l=NULL so fails */

      if(!l) {
        rasqal_free_row(row);
        failed = 1;
        goto tidy;
      }
      rasqal_row_set_value_at(row, column_i, l);
    }

    raptor_sequence_push(seq, row);
  }

  tidy:
  if(failed) {
    if(seq) {
      raptor_free_sequence(seq);
      seq = NULL;
    }
  }
  
  return seq;
}