
int sqlite3Fts5UnicodeIsdiacritic(int c);
int sqlite3Fts5UnicodeCategory(u32 iCode);
int sqlite3Fts5UnicodeCatParse(const char *zCat, u8 *aArray);
void sqlite3Fts5UnicodeAscii(u8 *aArray, u8 *aAscii);
int sqlite3Fts5UnicodeFold(int c, int eRemoveDiacritic);

