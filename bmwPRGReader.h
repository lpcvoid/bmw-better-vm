//
// Created by lpcvoid on 2020-02-06.
//

#ifndef BESTVMTEST_BMWPRGREADER_H
#define BESTVMTEST_BMWPRGREADER_H


#include <queue>
#include <string>
#include "bmwBESTObject.h"
#include "../common/bmwFileBuffer.h"

#define BMW_PRG_READER_LOAD_FLAGS_CODE 1
#define BMW_PRG_READER_LOAD_FLAGS_TABLES 2
#define BMW_PRG_READER_LOAD_FLAGS_ALL 0xFFFFFFFF

bmwBESTObject*  ReadBESTObject(std::string filepath);


#endif //BESTVMTEST_BMWPRGREADER_H
