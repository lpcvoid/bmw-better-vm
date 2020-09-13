//
// Created by lpcvoid on 2020-02-22.
//

#include "bmwBESTVMTrace.hpp"
#include "../common/bmwTextFile.hpp"
#include "../common/bmwStringHelper.hpp"
#include "../common/bmwLogger.h"
#include <iostream>
#include <fstream>
#include <cstring>

//5 bits for the flag
//9 bits for the error
#define GENERATECOMPARERESULT(flag, error) ((((0x3eff & (uint16)flag) << 9)) | (0x1ff & (uint16)error))

#define GETCOMPARERESULT(flag) (0x3eff & (uint16)(flag >> 9))

#define GETCOMPARERESULTERROR(error) (0x1ff & (uint16)(error))

bool bmwBESTVMTrace::LoadTraceFile(std::string filepath) {
    std::string str_trace_file, obj_name;
    std::vector<uint8> resp;
    std::vector<std::string> resp_str, split;

    bmwTextFile tf;


    if (tf.LoadFile(filepath)) {


        for (int i = 0; i < tf.GetLineCount(); i++) {
            //std::cout << std::to_string(i) << std::endl;
            if (tf[i][0] == '#') {
                // input and output traces
                if (bmwStringHelper::contains(tf[i], "RECV")) {
                    if (bmwStringHelper::contains(tf[i], "COMPLEX")) {
                        // ignore this line, this job needs more things simulated
                    } else {
                        // contsins single response, most jobs will
                        resp_str = bmwStringHelper::split(bmwStringHelper::trim(tf[i]), ":");
                        bmwStringHelper::replace(resp_str[1], " ", "");
                        resp = bmwStringHelper::to_byte_array(resp_str[1]);
                        _trace_ecu_responses.push_back(resp);
                    }
                } else if (bmwStringHelper::contains(tf[i], "INPUT")) {
                    // ignore for now
                } else if (bmwStringHelper::contains(tf[i], "OUTPUT")) {
                    resp_str = bmwStringHelper::split(tf[i], ":");
                    resp = bmwStringHelper::to_byte_array(resp_str[1]);
                    _trace_ecu_responses.push_back(resp);
                }

                if (bmwStringHelper::contains(tf[i], "FLAGS")) {
                    _trace_flags |= BMW_BESTVM_TRACE_FLAGS;
                    BMWLOG().log("Trace : Enabled flags");
                }

                if (bmwStringHelper::contains(tf[i], "INTREGS")) {
                    _trace_flags |= BMW_BESTVM_TRACE_REGS;
                    BMWLOG().log("Trace : Enabled registers");
                }

                if (bmwStringHelper::contains(tf[i], "ONLYCOMS")) {
                    _trace_flags = 0;
                    BMWLOG().log("Trace : Only using adapter I/O, no other tracec");
                }
            } else {
                bmwBESTVMTraceEntry tt;
                if (bmwStringHelper::contains(tf[i], ",")) {
                    split = bmwStringHelper::split(tf[i], ",");

                    tt.trace_addr = bmwStringHelper::hex_to_int<uint32>(
                            split[0]);// std::atoi(reinterpret_cast<const char *>(strtol(split[0].c_str(), NULL, 16)));

                    if (_trace_flags & BMW_BESTVM_TRACE_FLAGS) {
                        tt.trace_flag_z = split[1] == "1";
                        tt.trace_flag_c = split[2] == "1";
                        tt.trace_flag_s = split[3] == "1";
                        tt.trace_flag_o = split[4] == "1";
                    }
                    if (_trace_flags & BMW_BESTVM_TRACE_REGS) {
                        tt.int_regs[0] = std::stoul(split[5],nullptr,10);
                        tt.int_regs[1] = std::stoul(split[6],nullptr,10);
                        tt.int_regs[2] = std::stoul(split[7],nullptr,10);
                        tt.int_regs[3] = std::stoul(split[8],nullptr,10);
                        tt.int_regs[4] = std::stoul(split[9],nullptr,10);
                        tt.int_regs[5] = std::stoul(split[10],nullptr,10);
                        tt.int_regs[6] = std::stoul(split[11],nullptr,10);
                        tt.int_regs[7] = std::stoul(split[12],nullptr,10);
                    }
                } else {
                    tt.trace_addr = bmwStringHelper::hex_to_int<uint32>(tf[i]);
                }
                _trace_target.push_back(tt);
            }
        }
        BMWLOG().debug("Loaded trace file : " + filepath);
        return true;
    }

    return false;

}

bmwBESTVMTrace::bmwBESTVMTrace() {
    ResetTrace();
}

bmwBESTVMTrace::~bmwBESTVMTrace() {

}

bmwBESTVMTraceResult bmwBESTVMTrace::CheckTraceStep(bmwBESTVMTraceEntry *step_entry, bool increment_step) {
    if (_trace_target.size() > 0) {



        //add to current trace
        _trace_current.push_back(step_entry->trace_addr);

        //if no trace flags reset, we are probably only interested in com trace, so trace always succeeds
        if (!_trace_flags)
            return bmwBESTVMTraceResult_trace_okay;

        if (_trace_index < _trace_target.size()) {

            uint32 match = _trace_target[_trace_index].CompareTraceEntry(step_entry, _trace_flags);
            uint32 flag = GETCOMPARERESULT(match);
            uint32 error = GETCOMPARERESULTERROR(match);

            if (flag & BMW_BESTVM_TRACE_ADDR) {
                BMWLOG().error("Trace mismatch at address " +
                               bmwStringHelper::int_to_hex<uint32>(_trace_target[_trace_index].trace_addr),
                               __FUNCTION__);
                return bmwBESTVMTraceResult_trace_mismatch;
            }

            if (flag & BMW_BESTVM_TRACE_FLAGS) {
                BMWLOG().error("Trace FLAG mismatch at address " +
                               bmwStringHelper::int_to_hex<uint32>(_trace_target[_trace_index].trace_addr),
                               __FUNCTION__);

                return bmwBESTVMTraceResult_trace_mismatch_flags;
            }

            if (flag & BMW_BESTVM_TRACE_REGS) {
                BMWLOG().error("Trace REGS mismatch at address " +
                               bmwStringHelper::int_to_hex<uint32>(_trace_target[_trace_index].trace_addr),
                               __FUNCTION__);
                BMWLOG().log("Register different : RL" + std::to_string(error));
                BMWLOG().log("Expected value : " +
                             bmwStringHelper::int_to_hex<uint32>(_trace_target[_trace_index].int_regs[error]));
                BMWLOG().log("Got value : " + bmwStringHelper::int_to_hex<uint32>(step_entry->int_regs[error]));
                return bmwBESTVMTraceResult_trace_mismatch_regs;
            }

            //increment if all went well
            if (increment_step)
                _trace_index++;

            //nobody else returned, all okay
            return bmwBESTVMTraceResult_trace_okay;
        } else {
            BMWLOG().error(std::string(__FUNCTION__) + " :: Trace ended before finishing!");
            return bmwBESTVMTraceResult_trace_eof;
        }
    } else return bmwBESTVMTraceResult_trace_not_loaded;


}

std::vector<uint8> bmwBESTVMTrace::GetResponseStep(bool increment_step) {
    if (_trace_ecu_responses.size() > 0) {


        if (_trace_ecu_response_index < _trace_ecu_responses.size()) {
            return _trace_ecu_responses[_trace_ecu_response_index++];

        } else {
            BMWLOG().error("Trace ended before finishing!", __FUNCTION__);
        }
    }

    return {};
}

void bmwBESTVMTrace::ResetTrace() {
    _trace_ecu_response_index = 0;
    _trace_index = 0;
}

std::vector<uint32> bmwBESTVMTrace::GetRecordedTrace() {
    return _trace_current;
}

bool bmwBESTVMTrace::ContainsTraceEntry(uint32 trace_eip) {
    for (bmwBESTVMTraceEntry target : _trace_target) {
        if (target.trace_addr == trace_eip)
            return true;
    }
    return false;
}

void bmwBESTVMTrace::SetTraceFlags(uint32_t traceFlags) {
    _trace_flags = traceFlags;
}

uint32 bmwBESTVMTraceEntry::CompareTraceEntry(bmwBESTVMTraceEntry *other, uint32 flags) {
    if (flags & BMW_BESTVM_TRACE_ADDR) {
        if (this->trace_addr != other->trace_addr)
            return GENERATECOMPARERESULT(BMW_BESTVM_TRACE_ADDR, 0);
    }

    if (flags & BMW_BESTVM_TRACE_FLAGS) {
        if (this->trace_flag_c != other->trace_flag_c)
            return GENERATECOMPARERESULT(BMW_BESTVM_TRACE_FLAGS, 1);
        if (this->trace_flag_o != other->trace_flag_o)
            return GENERATECOMPARERESULT(BMW_BESTVM_TRACE_FLAGS, 2);
        if (this->trace_flag_s != other->trace_flag_s)
            return GENERATECOMPARERESULT(BMW_BESTVM_TRACE_FLAGS, 4);
        if (this->trace_flag_z != other->trace_flag_z)
            return GENERATECOMPARERESULT(BMW_BESTVM_TRACE_FLAGS, 8);
    }

    if (flags & BMW_BESTVM_TRACE_REGS) {
        for (int i = 0; i < 8; i++)
            if (int_regs[i] != other->int_regs[i])
                return GENERATECOMPARERESULT(BMW_BESTVM_TRACE_REGS, i);
    }

    return 0;

}
