//
// Created by lpcvoid on 2020-02-06.
//

#ifndef BESTVMTEST_BMWBESTOBJECT_H
#define BESTVMTEST_BMWBESTOBJECT_H


#include <string>
#include <vector>
#include <map>

#include "../common/bmwTypes.h"
#include "bmwBESTTable.h"
#include "bmwBESTJob.h"
#include "../common/bmwMemoryBuffer.hpp"

#pragma pack(push, 1)
typedef struct { uint8 v[3]; } bmwBESTVerison;
typedef struct { uint16 v[2]; } bmwBESTRevision;
#pragma pack(pop)

enum bmwBESTDescriptionEntryType {
    bmwBESTDescriptionEntryType_result, bmwBESTDescriptionEntryType_result_type, bmwBESTDescriptionEntryType_result_comment
};


struct bmwBESTDescriptionEntry {
    std::string desc_name;
    std::string desc_comment;
    std::vector<std::pair<bmwBESTDescriptionEntryType, std::string>> desc_lines;
};


struct bmwBESTObject {
    bmwBESTRevision rev;
    bmwBESTVerison version;
    std::string last_changed;
    std::string filename;
    std::map<std::string, std::string> desc;
    std::map<std::string, bmwBESTDescriptionEntry*> desc_jobs;
    std::map<std::string, bmwBESTTable*> tables;
    std::map<std::string, bmwBESTJob*> jobs;
};

bool SerializeBESTObject(bmwMemoryBuffer& buf, bmwBESTObject* bo);
bmwBESTObject* DeserializeBESTObject(bmwMemoryBuffer& buf);


#endif //BESTVMTEST_BMWBESTOBJECT_H
