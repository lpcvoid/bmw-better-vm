//
// Created by lpcvoid on 2020-02-06.
//

#ifndef BESTVMTEST_BMWBESTJOB_H
#define BESTVMTEST_BMWBESTJOB_H


#include <string>
#include "../common/bmwTypes.h"
#include "bmwBESTVMDissasembly.h"

class bmwBESTJob {
    public:
        std::string name;
        std::vector<uint8> job_code;
        bmwBESTVMDissasembly dissasembly;
        bmwBESTJob();
};


#endif //BESTVMTEST_BMWBESTJOB_H
