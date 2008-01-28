/**
 * Copyright (c) 2005 Zed A. Shaw
 * You can redistribute it and/or modify it under the same terms as Ruby.
 */
#include "ruby.h"
#include "ext_help.h"
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include "tst.h"
#include <string.h>

typedef struct BMHSearchState {
  size_t occ[UCHAR_MAX+1];
  size_t htotal;
  unsigned char *needle;
  size_t nlen;
  size_t skip;
  size_t max_find;

  size_t *found_at;
  size_t nfound;
} BMHSearchState;

static VALUE mMongrel;
static VALUE cBMHSearch;
static VALUE eBMHSearchError;

inline size_t max(size_t a, size_t b) { return a>b ? a : b; }

void BMHSearch_free(void *data) {
  BMHSearchState *S = (BMHSearchState *)data;

  if(data) {
    if(S->needle) free(S->needle);
    if(S->found_at) free(S->found_at);
    free(data);
  }
}


VALUE BMHSearch_alloc(VALUE klass)
{
  VALUE obj;

  BMHSearchState *S = ALLOC_N(BMHSearchState, 1);

  obj = Data_Wrap_Struct(klass, NULL, BMHSearch_free, S);

  return obj;
}

/**
 * call-seq:
 *    BMHSearch.new(needle, max_find) -> searcher
 *
 * Prepares the needle for searching and allocates enough space to 
 * retain max_find locations.  BMHSearch will throw an exception
 * if you go over the max_find without calling BMHSearch#pop.
 */
VALUE BMHSearch_init(VALUE self, VALUE needle, VALUE max_find)
{
  BMHSearchState *S = NULL;
  size_t a, b;

  DATA_GET(self, BMHSearchState, S);

  S->htotal = 0;
  S->nfound = 0;
  S->skip = 0;
  S->nlen = RSTRING(needle)->len;
  S->max_find = FIX2INT(max_find);
  S->needle = NULL;
  S->found_at = NULL;

  if(S->nlen <= 0) rb_raise(eBMHSearchError, "Needle can't be 0 length.");

  // copy it so we don't have to worry about Ruby messing it up
  S->needle = ALLOC_N(unsigned char, S->nlen);
  memcpy(S->needle, RSTRING(needle)->ptr, S->nlen);

  // setup the number of finds they want
  S->found_at = ALLOC_N(size_t, S->max_find);

  /* Preprocess */
  /* Initialize the table to default value */
  for(a=0; a<UCHAR_MAX+1; a=a+1) S->occ[a] = S->nlen;

  /* Then populate it with the analysis of the needle */
  b=S->nlen;
  for(a=0; a < S->nlen; a=a+1)
  {
    b=b-1;
    S->occ[S->needle[a]] = b;
  }

  return self;
}

/**
 * call-seq:
 *    searcher.find(haystack) -> nfound
 *
 * This will look for the needle in the haystack and return the total
 * number found so far.  The key ingredient is that you can pass in 
 * as many haystacks as you want as long as they are longer than the
 * needle.  Each time you call find, it adds to the total and number
 * found.  The purpose is to allow you to read a large string in chunks
 * and find the needle.
 *
 * The main purpose of the nfound is so that you can periodically call
 * BMHSearch.pop to clear the list of found locations before you run
 * out of max_finds.
 */
VALUE BMHSearch_find(VALUE self, VALUE hay)
{
  size_t hpos = 0, npos = 0, skip = 0;
  unsigned char* haystack = NULL;
  size_t hlen = 0, last = 0;
  BMHSearchState *S = NULL;

  DATA_GET(self, BMHSearchState, S);

  haystack = RSTRING(hay)->ptr;
  hlen = RSTRING(hay)->len;

  /* Sanity checks on the parameters */
  if(S->nlen > hlen) {
    rb_raise(eBMHSearchError, "Haystack can't be smaller than needle.");
  } else if(S->nlen <= 0) {
    rb_raise(eBMHSearchError, "Needle can't be 0 length.");
  } else if(!haystack || !S->needle) {
    rb_raise(eBMHSearchError, "Corrupt search state. REALLY BAD!");
  }

  /* Check for a trailing remainder, which is only possible if skip > 1 */
  if(S->skip) {
    // only scan for what should be the rest of the string
    size_t remainder = S->nlen - S->skip;
    if(!memcmp(haystack, S->needle + S->skip, remainder)) {
      // record a find
      S->found_at[S->nfound++] = S->htotal - S->skip;
      // move up by the amount found 
      hpos += remainder; 
    }
  }


  /* Start searching from the end of S->needle (this is not a typo) */
  hpos = S->nlen-1;

  while(hpos < hlen) {
    /* Compare the S->needle backwards, and stop when first mismatch is found */
    npos = S->nlen-1;

    while(hpos < hlen && haystack[hpos] == S->needle[npos]) {
      if(npos == 0) {
        // found one, log it in found_at, but reduce by the skip if we're at the beginning
        S->found_at[S->nfound++] = hpos + S->htotal;
        last = hpos;  // keep track of the last one we found in this chunk

        if(S->nfound > S->max_find) {
          rb_raise(eBMHSearchError, "More than max requested needles found. Use pop.");
        }

        /* continue at the next possible spot, from end of S->needle (not typo) */
        hpos += S->nlen + S->nlen - 1; 
        npos = S->nlen - 1;
        continue;
      } else {
        hpos--;
        npos--;
      }
    }

    // exhausted the string already, done
    if(hpos > hlen) break;

    /* Find out how much ahead we can skip based on the byte that was found */
    skip = max(S->nlen - npos, S->occ[haystack[hpos]]);
    hpos += skip;
  }


  skip = 0;  // skip defaults to 0

  // don't bother if the string ends in the needle completely
  if(S->nfound == 0 || last != hlen - S->nlen) {
    // invert the occ array to figure out how much to jump back
    size_t back = S->nlen - S->occ[haystack[hlen-1]];

    // the needle could have an ending char that's also repeated in the needle
    // in that case, we have to adjust to search for nlen-1 just in case
    if(haystack[hlen-1] == S->needle[S->nlen - 1]) {
      back = S->nlen - 1;
    }

    int found_it = 0;  // defaults to 0 for test at end of it
    if(back < S->nlen && back > 0) {
      // search for the longest possible prefix
      for(hpos = hlen - back; hpos < hlen; hpos++) {
        skip = hlen - hpos;
        if(!memcmp(haystack+hpos, S->needle, skip)) {
          found_it = 1; // make sure we actually found it
          break;
        }
      }
    }
    // skip will be 1 if nothing was found, so we have to force it 0
    if(!found_it) skip = 0;
  }

  // keep track of the skip, it's set to 0 by the code above
  S->skip = skip;
  // update the total processed so far so indexes will be in sync
  S->htotal += hlen;

  return INT2FIX(S->nfound);
}


/**
 * call-seq:
 *    searcher.pop -> [1, 2, 3, ..]
 *
 * Pops off the locations in the TOTAL string (over all haystacks processed)
 * clearing the internal buffer for more space and letting you use it.
 */
VALUE BMHSearch_pop(VALUE self)
{
  int i = 0;
  BMHSearchState *S = NULL;
  VALUE result;

  DATA_GET(self, BMHSearchState, S);

  result = rb_ary_new();
  for(i = 0; i < S->nfound; i++) {
    rb_ary_push(result, INT2FIX(S->found_at[i]));
  }

  S->nfound = 0;
  
  return result;
}


/**
 * call-seq:
 *    searcher.nfound -> Fixed
 *
 * The number found so far.
 */
VALUE BMHSearch_nfound(VALUE self)
{
  BMHSearchState *S = NULL;
  DATA_GET(self, BMHSearchState, S);
  return INT2FIX(S->nfound);
}

/**
 * call-seq:
 *    searcher.max_find -> Fixed
 *
 * The current maximum find setting (cannot be changed).
 */
VALUE BMHSearch_max_find(VALUE self)
{
  BMHSearchState *S = NULL;
  DATA_GET(self, BMHSearchState, S);
  return INT2FIX(S->max_find);
}

/**
 * call-seq:
 *    searcher.total -> Fixed
 *
 * The total bytes processed so far.
 */
VALUE BMHSearch_total(VALUE self)
{
  BMHSearchState *S = NULL;
  DATA_GET(self, BMHSearchState, S);
  return INT2FIX(S->htotal);
}

/**
 * call-seq:
 *    searcher.needle -> String
 *
 * A COPY of the internal needle string being used for searching.
 */
VALUE BMHSearch_needle(VALUE self)
{
  BMHSearchState *S = NULL;
  DATA_GET(self, BMHSearchState, S);
  return rb_str_new(S->needle, S->nlen);
}

/**
 * call-seq:
 *    searcher.has_trailing? -> Boolean
 *
 * Tells you whether the last call to BMHSearch#find has possible
 * trailing matches.  This is mostly for completeness but you
 * might use this to adjust the amount of data you're reading
 * or the buffer size.
 */
VALUE BMHSearch_has_trailing(VALUE self)
{
  BMHSearchState *S = NULL;
  DATA_GET(self, BMHSearchState, S);
  return S->skip ? Qtrue : Qfalse;
}


void Init_bmh_search()
{

  mMongrel = rb_define_module("Mongrel");


  eBMHSearchError = rb_define_class_under(mMongrel, "BMHSearchError", rb_eIOError);

  cBMHSearch = rb_define_class_under(mMongrel, "BMHSearch", rb_cObject);
  rb_define_alloc_func(cBMHSearch, BMHSearch_alloc);
  rb_define_method(cBMHSearch, "initialize", BMHSearch_init,2);
  rb_define_method(cBMHSearch, "find", BMHSearch_find,1);
  rb_define_method(cBMHSearch, "pop", BMHSearch_pop,0);
  rb_define_method(cBMHSearch, "nfound", BMHSearch_nfound,0);
  rb_define_method(cBMHSearch, "total", BMHSearch_total,0);
  rb_define_method(cBMHSearch, "needle", BMHSearch_needle,0);
  rb_define_method(cBMHSearch, "max_find", BMHSearch_max_find,0);
  rb_define_method(cBMHSearch, "has_trailing?", BMHSearch_has_trailing,0);

}


