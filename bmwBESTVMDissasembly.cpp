//
// Created by lpcvoid on 2020-03-08.
//

#include <algorithm>
#include "bmwBESTVMDissasembly.h"

bmwBESTVMDissasembly::bmwBESTVMDissasembly() {
    clear();
}

bmwBESTVMDissasembly::~bmwBESTVMDissasembly() {
    for (bmwBESTVMDissasemblyToken* d : *this)
        delete d;
    clear();
}

std::vector<std::string> bmwBESTVMDissasembly::GetAllUsedInstructions() {
    std::vector<std::string> res;
    for (bmwBESTVMDissasemblyToken* d : *this){
        if ((std::find(std::begin(res), std::end(res), d->token_operation) == std::end(res)))
            res.push_back(d->token_operation);
    }

    return res;
}
