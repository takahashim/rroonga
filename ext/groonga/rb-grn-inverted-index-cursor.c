/* -*- coding: utf-8; mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
  Copyright (C) 2017  Kouhei Sutou <kou@clear-code.com>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License version 2.1 as published by the Free Software Foundation.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "rb-grn.h"

#define SELF(object) ((RbGrnInvertedIndexCursor *)DATA_PTR(object))

typedef struct _RbGrnInvertedIndexCursor RbGrnInvertedIndexCursor;
struct _RbGrnInvertedIndexCursor
{
    VALUE self;
    grn_ctx *context;
    grn_ii_cursor *cursor;
    grn_id term_id;
};

static VALUE rb_cGrnInvertedIndexCursor;

static void
rb_grn_inverted_index_cursor_free(void *data)
{
    RbGrnInvertedIndexCursor *rb_grn_cursor = data;

    if (rb_grn_cursor->context) {
        grn_ii_cursor_close(rb_grn_cursor->context,
                            rb_grn_cursor->cursor);
    }
    xfree(rb_grn_cursor);
}

static const rb_data_type_t rb_grn_inverted_index_cursor_type = {
    "Groonga::InvertedIndexCursor",
    {
        NULL,
        rb_grn_inverted_index_cursor_free,
        NULL,
    },
    0,
    0,
    RUBY_TYPED_FREE_IMMEDIATELY,
};

VALUE
rb_grn_inverted_index_cursor_to_ruby_object (grn_ctx *context,
                                             grn_ii_cursor *cursor,
                                             VALUE rb_table,
                                             VALUE rb_lexicon)
{
    VALUE rb_cursor;
    RbGrnInvertedIndexCursor *rb_grn_cursor;

    rb_cursor = TypedData_Make_Struct(rb_cGrnInvertedIndexCursor,
                                      RbGrnInvertedIndexCursor,
                                      &rb_grn_inverted_index_cursor_type,
                                      rb_grn_cursor);

    rb_grn_cursor->self = rb_cursor;
    rb_grn_cursor->context = context;
    rb_grn_cursor->cursor = cursor;
    rb_iv_set(rb_cursor, "@table", rb_table);
    rb_iv_set(rb_cursor, "@lexicon", rb_lexicon);

    return rb_cursor;
}

static VALUE
next_value (VALUE rb_posting,
            RbGrnInvertedIndexCursor *rb_grn_cursor,
            VALUE rb_table,
            VALUE rb_lexicon)
{
    grn_posting *posting;

    posting = grn_ii_cursor_next(rb_grn_cursor->context,
                                 rb_grn_cursor->cursor);
    if (!posting) {
        return Qnil;
    }

    if (NIL_P(rb_posting)) {
        return rb_grn_posting_new(posting,
                                  rb_grn_cursor->term_id,
                                  rb_table,
                                  rb_lexicon);
    } else {
        rb_grn_posting_update(rb_posting,
                              posting,
                              rb_grn_cursor->term_id);
        return rb_posting;
    }
}

static VALUE
rb_grn_inverted_index_cursor_next (VALUE self)
{
    RbGrnInvertedIndexCursor *rb_grn_cursor;
    VALUE rb_table;
    VALUE rb_lexicon;
    VALUE rb_posting;

    TypedData_Get_Struct(self,
                         RbGrnInvertedIndexCursor,
                         &rb_grn_inverted_index_cursor_type,
                         rb_grn_cursor);
    if (!rb_grn_cursor->context) {
        rb_raise(rb_eGrnClosed,
                 "can't access already closed Groonga object: %" PRIsVALUE,
                 self);
    }

    rb_table = rb_iv_get(self, "@table");
    rb_lexicon = rb_iv_get(self, "@lexicon");
    rb_posting = next_value(Qnil, rb_grn_cursor, rb_table, rb_lexicon);

    return rb_posting;

}

static VALUE
rb_grn_inverted_index_cursor_each (int argc, VALUE *argv, VALUE self)
{
    RbGrnInvertedIndexCursor *rb_grn_cursor;
    grn_bool reuse_posting_object;
    VALUE rb_options;
    VALUE rb_reuse_posting_object;
    VALUE rb_table;
    VALUE rb_lexicon;
    VALUE rb_posting = Qnil;

    RETURN_ENUMERATOR(self, argc, argv);

    rb_scan_args(argc, argv, "01", &rb_options);

    rb_grn_scan_options(rb_options,
                        "reuse_posting_object", &rb_reuse_posting_object,
                        NULL);

    TypedData_Get_Struct(self,
                         RbGrnInvertedIndexCursor,
                         &rb_grn_inverted_index_cursor_type,
                         rb_grn_cursor);
    if (!rb_grn_cursor->context) {
        rb_raise(rb_eGrnClosed,
                 "can't access already closed Groonga object: %" PRIsVALUE,
                 self);
    }

    rb_table = rb_iv_get(self, "@table");
    rb_lexicon = rb_iv_get(self, "@lexicon");
    reuse_posting_object = RVAL2CBOOL(rb_reuse_posting_object);

    if (reuse_posting_object) {
        rb_posting = rb_grn_posting_new(NULL, GRN_ID_NIL, rb_table, rb_lexicon);
    }
    while (GRN_TRUE) {
        if (!reuse_posting_object) {
            rb_posting = Qnil;
        }
        rb_posting = next_value(rb_posting, rb_grn_cursor, rb_table, rb_lexicon);
        if (NIL_P(rb_posting)) {
            break;
        }
        rb_yield(rb_posting);
    }

    return Qnil;
}

static VALUE
rb_grn_inverted_index_cursor_close (VALUE self)
{
    RbGrnInvertedIndexCursor *rb_grn_cursor;

    TypedData_Get_Struct(self,
                         RbGrnInvertedIndexCursor,
                         &rb_grn_inverted_index_cursor_type,
                         rb_grn_cursor);
    if (!rb_grn_cursor->context) {
        rb_raise(rb_eGrnClosed,
                 "can't access already closed Groonga object: %" PRIsVALUE,
                 self);
    }

    grn_ii_cursor_close(rb_grn_cursor->context,
                        rb_grn_cursor->cursor);
    rb_grn_cursor->context = NULL;
    rb_grn_cursor->cursor = NULL;

    return Qnil;

}

void
rb_grn_init_inverted_index_cursor (VALUE mGrn)
{
    rb_cGrnInvertedIndexCursor =
      rb_define_class_under(mGrn, "InvertedIndexCursor", rb_cData);
    rb_include_module(rb_cGrnInvertedIndexCursor, rb_mEnumerable);

    rb_define_method(rb_cGrnInvertedIndexCursor, "next",
                     rb_grn_inverted_index_cursor_next, 0);
    rb_define_method(rb_cGrnInvertedIndexCursor, "each",
                     rb_grn_inverted_index_cursor_each, -1);
    rb_define_method(rb_cGrnInvertedIndexCursor, "close",
                     rb_grn_inverted_index_cursor_close, 0);
}
