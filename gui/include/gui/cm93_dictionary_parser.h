/**************************************************************************
 * Copyright (C) 2026 OpenCPN Authors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 **************************************************************************/

#ifndef GUI_CM93_DICTIONARY_PARSER_H_
#define GUI_CM93_DICTIONARY_PARSER_H_

#include <cstddef>

#include <wx/string.h>
#include <wx/tokenzr.h>

/** CM93 object and attribute identifiers are stored as one unsigned byte. */
constexpr int kCm93MaxDictionaryIndex = 255;

/**
 * Extract and validate the numeric identifier from a CM93 dictionary record.
 *
 * Blank lines, comments, truncated records, non-numeric identifiers and
 * identifiers which cannot occur in a CM93 data record are rejected.  Keeping
 * this validation independent of allocation prevents malformed dictionaries
 * from selecting an unbounded lookup-table size.
 */
inline bool ParseCm93DictionaryRecordIndex(const wxString& line,
                                           std::size_t minimum_fields,
                                           int* index) {
  if (!index || line.empty() || line.StartsWith(";")) return false;

  wxStringTokenizer tokenizer(line, "|\r\n");
  if (tokenizer.CountTokens() < minimum_fields) return false;

  tokenizer.GetNextToken();  // Object/attribute label.
  const wxString token = tokenizer.GetNextToken();
  long parsed = 0;
  if (!token.ToLong(&parsed) || parsed <= 0 ||
      parsed > kCm93MaxDictionaryIndex) {
    return false;
  }

  *index = static_cast<int>(parsed);
  return true;
}

#endif  // GUI_CM93_DICTIONARY_PARSER_H_
