/* Add your header comment here */
#include "sqlite3ext.h" /* Do not use <sqlite3.h>! */
SQLITE_EXTENSION_INIT1

#include "uni.h"

/* Insert your extension code here */
static int fts5_api_from_db(sqlite3 *db, fts5_api **ppApi);
typedef int (*xTokenFn)(void *, int, const char *, int, int, int);
int fts5_simper_xCreate(void *sqlite3, const char **azArg, int nArg, Fts5Tokenizer **ppOut);
int fts5_simper_xTokenize(Fts5Tokenizer *tokenizer_ptr, void *pCtx, int flags, const char *pText, int nText,
                          xTokenFn xToken);
void fts5_simper_xDelete(Fts5Tokenizer *p);


#ifdef _WIN32
__declspec(dllexport)
#endif
/* TODO: Change the entry point name so that "extension" is replaced by
** text derived from the shared library filename as follows:  Copy every
** ASCII alphabetic character from the filename after the last "/" through
** the next following ".", converting each character to lowercase, and
** discarding the first three characters if they are "lib".
*/
int sqlite3_extension_init(
  sqlite3 *db, 
  char **pzErrMsg, 
  const sqlite3_api_routines *pApi
){
  int rc = SQLITE_OK;
  SQLITE_EXTENSION_INIT2(pApi);
  /* Insert here calls to
  **     sqlite3_create_function_v2(),
  **     sqlite3_create_collation_v2(),
  **     sqlite3_create_module_v2(), and/or
  **     sqlite3_vfs_register()
  ** to register the new features that your extension adds.
  */
  fts5_api *fts5api;
  if (rc = fts5_api_from_db(db, &fts5api) != SQLITE_OK) {
      return SQLITE_ERROR;
  }
  fts5_tokenizer tokenizer = {fts5UnicodeCreate, fts5UnicodeDelete, fts5UnicodeTokenize};

  rc = fts5api->xCreateTokenizer(fts5api, "simper", (void*)fts5api, &tokenizer, NULL);
  return rc;
}

/*
** Return a pointer to the fts5_api pointer for database connection db.
** If an error occurs, return NULL and leave an error in the database 
** handle (accessible using sqlite3_errcode()/errmsg()).
*/
static int fts5_api_from_db(sqlite3 *db, fts5_api **ppApi){
  sqlite3_stmt *pStmt = 0;
  if( SQLITE_OK==sqlite3_prepare_v2(db, "SELECT fts5(?1)", -1, &pStmt, 0) ){
    sqlite3_bind_pointer(pStmt, 1, (void*)ppApi, "fts5_api_ptr", NULL);
    sqlite3_step(pStmt);
  }
  return sqlite3_finalize(pStmt);
}

// int fts5_simper_xCreate(void *sqlite3, const char **azArg, int nArg, Fts5Tokenizer **ppOut) {
//   (void)sqlite3;
//   return SQLITE_OK;
// }

// int fts5_simper_xTokenize(Fts5Tokenizer *tokenizer_ptr, void *pCtx, int flags, const char *pText, int nText,
//                           xTokenFn xToken) {
//   // return tokenize(pCtx, flags, pText, nText, xToken);
// }

// void fts5_simper_xDelete(Fts5Tokenizer *p) {

// }
