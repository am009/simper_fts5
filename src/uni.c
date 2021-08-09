/* Add your header comment here */
#include "sqlite3ext.h" /* Do not use <sqlite3.h>! */
SQLITE_EXTENSION_INIT3

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>


typedef unsigned char  u8;
typedef unsigned int   u32;
typedef unsigned short u16;
typedef short i16;
typedef sqlite3_int64 i64;
typedef sqlite3_uint64 u64;
#ifndef UNUSED_PARAM
# define UNUSED_PARAM(X)  (void)(X)
#endif


#include "fts5_unicode2.h"

/**************************************************************************
** Start of unicode61 tokenizer implementation.
*/


/*
** The following two macros - READ_UTF8 and WRITE_UTF8 - have been copied
** from the sqlite3 source file utf.c. If this file is compiled as part
** of the amalgamation, they are not required.
*/
#ifndef SQLITE_AMALGAMATION

static const unsigned char sqlite3Utf8Trans1[] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x00, 0x00,
};

#define READ_UTF8(zIn, zTerm, c)                           \
  c = *(zIn++);                                            \
  if( c>=0xc0 ){                                           \
    c = sqlite3Utf8Trans1[c-0xc0];                         \
    while( zIn!=zTerm && (*zIn & 0xc0)==0x80 ){            \
      c = (c<<6) + (0x3f & *(zIn++));                      \
    }                                                      \
    if( c<0x80                                             \
        || (c&0xFFFFF800)==0xD800                          \
        || (c&0xFFFFFFFE)==0xFFFE ){  c = 0xFFFD; }        \
  }


#define WRITE_UTF8(zOut, c) {                          \
  if( c<0x00080 ){                                     \
    *zOut++ = (unsigned char)(c&0xFF);                 \
  }                                                    \
  else if( c<0x00800 ){                                \
    *zOut++ = 0xC0 + (unsigned char)((c>>6)&0x1F);     \
    *zOut++ = 0x80 + (unsigned char)(c & 0x3F);        \
  }                                                    \
  else if( c<0x10000 ){                                \
    *zOut++ = 0xE0 + (unsigned char)((c>>12)&0x0F);    \
    *zOut++ = 0x80 + (unsigned char)((c>>6) & 0x3F);   \
    *zOut++ = 0x80 + (unsigned char)(c & 0x3F);        \
  }else{                                               \
    *zOut++ = 0xF0 + (unsigned char)((c>>18) & 0x07);  \
    *zOut++ = 0x80 + (unsigned char)((c>>12) & 0x3F);  \
    *zOut++ = 0x80 + (unsigned char)((c>>6) & 0x3F);   \
    *zOut++ = 0x80 + (unsigned char)(c & 0x3F);        \
  }                                                    \
}

#endif /* ifndef SQLITE_AMALGAMATION */

typedef struct Unicode61Tokenizer Unicode61Tokenizer;
struct Unicode61Tokenizer {
  unsigned char aTokenChar[128];  /* ASCII range token characters */
  char *aFold;                    /* Buffer to fold text into */
  int nFold;                      /* Size of aFold[] in bytes */
  int eRemoveDiacritic;           /* True if remove_diacritics=1 is set */
  int nException;
  int *aiException;

  unsigned char aCategory[32];    /* True for token char categories */
};

/* Values for eRemoveDiacritic (must match internals of fts5_unicode2.c) */
#define FTS5_REMOVE_DIACRITICS_NONE    0
#define FTS5_REMOVE_DIACRITICS_SIMPLE  1
#define FTS5_REMOVE_DIACRITICS_COMPLEX 2

static int fts5UnicodeAddExceptions(
  Unicode61Tokenizer *p,          /* Tokenizer object */
  const char *z,                  /* Characters to treat as exceptions */
  int bTokenChars                 /* 1 for 'tokenchars', 0 for 'separators' */
){
  int rc = SQLITE_OK;
  int n = (int)strlen(z);
  int *aNew;

  if( n>0 ){
    aNew = (int*)sqlite3_realloc64(p->aiException,
                                   (n+p->nException)*sizeof(int));
    if( aNew ){
      int nNew = p->nException;
      const unsigned char *zCsr = (const unsigned char*)z;
      const unsigned char *zTerm = (const unsigned char*)&z[n];
      while( zCsr<zTerm ){
        u32 iCode;
        int bToken;
        READ_UTF8(zCsr, zTerm, iCode);
        if( iCode<128 ){
          p->aTokenChar[iCode] = (unsigned char)bTokenChars;
        }else{
          bToken = p->aCategory[sqlite3Fts5UnicodeCategory(iCode)];
          assert( (bToken==0 || bToken==1) ); 
          assert( (bTokenChars==0 || bTokenChars==1) );
          if( bToken!=bTokenChars && sqlite3Fts5UnicodeIsdiacritic(iCode)==0 ){
            int i;
            for(i=0; i<nNew; i++){
              if( (u32)aNew[i]>iCode ) break;
            }
            memmove(&aNew[i+1], &aNew[i], (nNew-i)*sizeof(int));
            aNew[i] = iCode;
            nNew++;
          }
        }
      }
      p->aiException = aNew;
      p->nException = nNew;
    }else{
      rc = SQLITE_NOMEM;
    }
  }

  return rc;
}

/*
** Return true if the p->aiException[] array contains the value iCode.
*/
static int fts5UnicodeIsException(Unicode61Tokenizer *p, int iCode){
  if( p->nException>0 ){
    int *a = p->aiException;
    int iLo = 0;
    int iHi = p->nException-1;

    while( iHi>=iLo ){
      int iTest = (iHi + iLo) / 2;
      if( iCode==a[iTest] ){
        return 1;
      }else if( iCode>a[iTest] ){
        iLo = iTest+1;
      }else{
        iHi = iTest-1;
      }
    }
  }

  return 0;
}

/*
** Delete a "unicode61" tokenizer.
*/
void fts5UnicodeDelete(Fts5Tokenizer *pTok){
  if( pTok ){
    Unicode61Tokenizer *p = (Unicode61Tokenizer*)pTok;
    sqlite3_free(p->aiException);
    sqlite3_free(p->aFold);
    sqlite3_free(p);
  }
  return;
}

static int unicodeSetCategories(Unicode61Tokenizer *p, const char *zCat){
  const char *z = zCat;

  while( *z ){
    while( *z==' ' || *z=='\t' ) z++;
    if( *z && sqlite3Fts5UnicodeCatParse(z, p->aCategory) ){
      return SQLITE_ERROR;
    }
    while( *z!=' ' && *z!='\t' && *z!='\0' ) z++;
  }

  sqlite3Fts5UnicodeAscii(p->aCategory, p->aTokenChar);
  return SQLITE_OK;
}

/*
** Create a "unicode61" tokenizer.
*/
int fts5UnicodeCreate(
  void *pUnused, 
  const char **azArg, int nArg,
  Fts5Tokenizer **ppOut
){
  int rc = SQLITE_OK;             /* Return code */
  Unicode61Tokenizer *p = 0;      /* New tokenizer object */ 

  UNUSED_PARAM(pUnused);

  if( nArg%2 ){
    rc = SQLITE_ERROR;
  }else{
    p = (Unicode61Tokenizer*)sqlite3_malloc(sizeof(Unicode61Tokenizer));
    if( p ){
      const char *zCat = "L* N* Co";
      int i;
      memset(p, 0, sizeof(Unicode61Tokenizer));

      p->eRemoveDiacritic = FTS5_REMOVE_DIACRITICS_SIMPLE;
      p->nFold = 64;
      p->aFold = sqlite3_malloc64(p->nFold * sizeof(char));
      if( p->aFold==0 ){
        rc = SQLITE_NOMEM;
      }

      /* Search for a "categories" argument */
      for(i=0; rc==SQLITE_OK && i<nArg; i+=2){
        if( 0==sqlite3_stricmp(azArg[i], "categories") ){
          zCat = azArg[i+1];
        }
      }

      if( rc==SQLITE_OK ){
        rc = unicodeSetCategories(p, zCat);
      }

      for(i=0; rc==SQLITE_OK && i<nArg; i+=2){
        const char *zArg = azArg[i+1];
        if( 0==sqlite3_stricmp(azArg[i], "remove_diacritics") ){
          if( (zArg[0]!='0' && zArg[0]!='1' && zArg[0]!='2') || zArg[1] ){
            rc = SQLITE_ERROR;
          }else{
            p->eRemoveDiacritic = (zArg[0] - '0');
            assert( p->eRemoveDiacritic==FTS5_REMOVE_DIACRITICS_NONE
                 || p->eRemoveDiacritic==FTS5_REMOVE_DIACRITICS_SIMPLE
                 || p->eRemoveDiacritic==FTS5_REMOVE_DIACRITICS_COMPLEX
            );
          }
        }else
        if( 0==sqlite3_stricmp(azArg[i], "tokenchars") ){
          rc = fts5UnicodeAddExceptions(p, zArg, 1);
        }else
        if( 0==sqlite3_stricmp(azArg[i], "separators") ){
          rc = fts5UnicodeAddExceptions(p, zArg, 0);
        }else
        if( 0==sqlite3_stricmp(azArg[i], "categories") ){
          /* no-op */
        }else{
          rc = SQLITE_ERROR;
        }
      }

    }else{
      rc = SQLITE_NOMEM;
    }
    if( rc!=SQLITE_OK ){
      fts5UnicodeDelete((Fts5Tokenizer*)p);
      p = 0;
    }
    *ppOut = (Fts5Tokenizer*)p;
  }
  return rc;
}

/*
** Return true if, for the purposes of tokenizing with the tokenizer
** passed as the first argument, codepoint iCode is considered a token 
** character (not a separator).
*/
static int fts5UnicodeIsAlnum(Unicode61Tokenizer *p, int iCode){
  return (
    p->aCategory[sqlite3Fts5UnicodeCategory((u32)iCode)]
    ^ fts5UnicodeIsException(p, iCode)
  );
}

int fts5UnicodeTokenize(
  Fts5Tokenizer *pTokenizer,
  void *pCtx,
  int iUnused,
  const char *pText, int nText,
  int (*xToken)(void*, int, const char*, int nToken, int iStart, int iEnd)
){
  Unicode61Tokenizer *p = (Unicode61Tokenizer*)pTokenizer;
  int rc = SQLITE_OK;
  unsigned char *a = p->aTokenChar;

  unsigned char *zTerm = (unsigned char*)&pText[nText];
  unsigned char *zCsr = (unsigned char *)pText;

  /* Output buffer */
  char *aFold = p->aFold;
  int nFold = p->nFold;
  const char *pEnd = &aFold[nFold-6];

  UNUSED_PARAM(iUnused);

  /* Each iteration of this loop gobbles up a contiguous run of separators,
  ** then the next token.  */
  while( rc==SQLITE_OK ){
    u32 iCode;                    /* non-ASCII codepoint read from input */
    char *zOut = aFold;
    int is;
    int ie;

    /* Skip any separator characters. */
    while( 1 ){
      if( zCsr>=zTerm ) goto tokenize_done;
      if( *zCsr & 0x80 ) {
        /* A character outside of the ascii range. Skip past it if it is
        ** a separator character. Or break out of the loop if it is not. */
        is = zCsr - (unsigned char*)pText;
        READ_UTF8(zCsr, zTerm, iCode);
        if( fts5UnicodeIsAlnum(p, iCode) ){
          goto non_ascii_tokenchar;
        }
      }else{
        if( a[*zCsr] ){
          is = zCsr - (unsigned char*)pText;
          goto ascii_tokenchar;
        }
        zCsr++;
      }
    }

    /* Run through the tokenchars. Fold them into the output buffer along
    ** the way.  */
    while( zCsr<zTerm ){

      /* Grow the output buffer so that there is sufficient space to fit the
      ** largest possible utf-8 character.  */
      if( zOut>pEnd ){
        aFold = sqlite3_malloc64((sqlite3_int64)nFold*2);
        if( aFold==0 ){
          rc = SQLITE_NOMEM;
          goto tokenize_done;
        }
        zOut = &aFold[zOut - p->aFold];
        memcpy(aFold, p->aFold, nFold);
        sqlite3_free(p->aFold);
        p->aFold = aFold;
        p->nFold = nFold = nFold*2;
        pEnd = &aFold[nFold-6];
      }

      if( *zCsr & 0x80 ){
        /* An non-ascii-range character. Fold it into the output buffer if
        ** it is a token character, or break out of the loop if it is not. */
        READ_UTF8(zCsr, zTerm, iCode);
        if( fts5UnicodeIsAlnum(p,iCode)||sqlite3Fts5UnicodeIsdiacritic(iCode) ){
 non_ascii_tokenchar:
          iCode = sqlite3Fts5UnicodeFold(iCode, p->eRemoveDiacritic);
          if( iCode ) WRITE_UTF8(zOut, iCode);
        }else{
          break;
        }
      }else if( a[*zCsr]==0 ){
        /* An ascii-range separator character. End of token. */
        break; 
      }else{
 ascii_tokenchar:
        if( *zCsr>='A' && *zCsr<='Z' ){
          *zOut++ = *zCsr + 32;
        }else{
          *zOut++ = *zCsr;
        }
        zCsr++;
      }
      ie = zCsr - (unsigned char*)pText;
    }

    /* Invoke the token callback */
    rc = xToken(pCtx, 0, aFold, zOut-aFold, is, ie); 
  }
  
 tokenize_done:
  if( rc==SQLITE_DONE ) rc = SQLITE_OK;
  return rc;
}
