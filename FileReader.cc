/*
 * Copyright ©2023 Travis McGaha.  All rights reserved.  Permission is
 * hereby granted to the students registered for University of Pennsylvania
 * CIT 5950 for use solely during Spring Semester 2023 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include <stdio.h>
#include <string>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <fstream>

#include "./HttpUtils.h"
#include "./FileReader.h"

using std::string;

namespace searchserver {

bool FileReader::read_file(string *str) {
    // Read the file into memory, and store the file contents in the
    // output parameter "str."  Be careful to handle binary data
    // correctly; i.e., you probably want to use the two-argument
    // constructor to std::string (the one that includes a length as a
    // second argument).

    // TODO: implement

    // Open the file
    std::ifstream file;
    file.open(fname_, std::ios::binary);

    // Check if the file is open
    if (!file.is_open()) {
        std::cerr << "Error: could not open file \"" << fname_ << "\"\n";
        return false;
    }

    // Get the length of the file
    file.seekg(0, std::ios::end);
    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Resize the string to fit the file contents
    str->resize(file_size);

    // Read the file into the string
    if (!file.read(&(*str)[0], file_size)) {
        std::cerr << "Error: could not read file \"" << fname_ << "\"\n";
        return false;
    }

    // Close the file
    file.close();

    return true;
}
}
