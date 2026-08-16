# Standard library convention

## Module declarations should be the same as the directory path they are in (slashes replaced be '::'), NOT the path of the file

eg.

file: std/core/assert.aria

CORRECT: module std::core;
INCORRECT: module std::core::assert;

file: std/io/file.aria

CORRECT: module std::io;
INCORRECT: module std::io::file;
