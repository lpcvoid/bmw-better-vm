//
// Created by lpcvoid on 2020-02-22.
//

#ifndef BESTVMTEST_BMWBESTVMTRACE_HPP
#define BESTVMTEST_BMWBESTVMTRACE_HPP


#include <vector>
#include <string>
#include "../common/bmwTypes.h"

enum bmwBESTVMTraceResult {    bmwBESTVMTraceResult_trace_okay,
    bmwBESTVMTraceResult_trace_mismatch_flags,
    bmwBESTVMTraceResult_trace_mismatch_regs,
    bmwBESTVMTraceResult_trace_mismatch,
    bmwBESTVMTraceResult_trace_response_mismatch,
    bmwBESTVMTraceResult_trace_eof,
    bmwBESTVMTraceResult_trace_not_loaded
};

struct bmwBESTVMTraceEntry {
    uint32 trace_addr;
    bool trace_flag_z;
    bool trace_flag_c;
    bool trace_flag_s;
    bool trace_flag_o;
    uint32 int_regs[8]; //there are 8 32 bit regs
    //compare to another trace using flags to determine what gets compared
    //returns 0 if trace is identical, otherwise sets the result flags of things that did not match
    //example : if BMW_BESTVM_TRACE_ADDR is set in flags, and registers do not match, functions sets BMW_BESTVM_TRACE_ADDR bits in results
    uint32 CompareTraceEntry(bmwBESTVMTraceEntry* other, uint32 flags);
};


#define BMW_BESTVM_TRACE_ADDR 1
#define BMW_BESTVM_TRACE_FLAGS 2
#define BMW_BESTVM_TRACE_REGS 4

class bmwBESTVMTrace {
private:
    std::vector<uint32> _trace_current;
    // list of known correct opcode addresses. Flow is compared and if errored, vm returns bmwBESTVMTraceResult_trace_mismatch. No action if empty
    std::vector<bmwBESTVMTraceEntry> _trace_target;
    //ecu respoinses as input for xsend opcodes
    std::vector<std::vector<uint8>> _trace_ecu_responses;
    //current response index, gets incremented on every xsend
    uint32 _trace_ecu_response_index;
    //current trace index
    uint32 _trace_index;
    //what is supposed to be traced
    uint32 _trace_flags = BMW_BESTVM_TRACE_ADDR;
public:
    void SetTraceFlags(uint32_t traceFlags);

public:

    bmwBESTVMTrace();
    virtual ~bmwBESTVMTrace();
    bool LoadTraceFile(std::string filepath);



    bmwBESTVMTraceResult CheckTraceStep(bmwBESTVMTraceEntry* step_entry, bool increment_step = true);
    std::vector<uint8> GetResponseStep( bool increment_step = true);

    std::vector<uint32> GetRecordedTrace();
    bool ContainsTraceEntry(uint32 trace_eip);

    void ResetTrace();


};


#endif //BESTVMTEST_BMWBESTVMTRACE_HPP
