int fts5UnicodeTokenize(
  Fts5Tokenizer *pTokenizer,
  void *pCtx,
  int iUnused,
  const char *pText, int nText,
  int (*xToken)(void*, int, const char*, int nToken, int iStart, int iEnd)
);

int fts5UnicodeCreate(
  void *pUnused, 
  const char **azArg, int nArg,
  Fts5Tokenizer **ppOut
);

void fts5UnicodeDelete(Fts5Tokenizer *pTok);


