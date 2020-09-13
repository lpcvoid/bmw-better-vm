//
// Created by lpcvoid on 2020-02-06.
//
#include "bmwBESTObject.h"
#include "../common/bmwStringHelper.hpp"


bool SerializeBESTObject(bmwMemoryBuffer &buf, bmwBESTObject *bo) {
    buf.write<bmwBESTRevision>(bo->rev);
    buf.write<bmwBESTVerison>(bo->version);
    buf.write_zero_terminated_string(bo->last_changed);
    buf.write_zero_terminated_string(bo->filename);
    buf.write<uint32>(bo->desc.size());
    for (auto& p : bo->desc){
        buf.write_zero_terminated_string(p.first);
        buf.write_zero_terminated_string(p.second);
    }

    buf.write<uint32>(bo->desc_jobs.size());
    for (auto& p : bo->desc_jobs){
        buf.write_zero_terminated_string(p.first);
        buf.write_zero_terminated_string(p.second->desc_name);
        buf.write_zero_terminated_string(p.second->desc_comment);

        buf.write<uint32>(p.second->desc_lines.size());
        for (auto l : p.second->desc_lines){
            buf.write<bmwBESTDescriptionEntryType>(l.first);
            buf.write_zero_terminated_string(l.second);
        }
    }
    buf.write<uint32>(bo->tables.size());
    for (auto& t : bo->tables){
        buf.write_zero_terminated_string(t.first);
        buf.write<uint32>(t.second->table_head.size());
        for (auto h : t.second->table_head){
            buf.write_zero_terminated_string(h);
        }
        buf.write<uint32>(t.second->table_data.size()); //row count
        buf.write<uint32>(t.second->table_data.front().size()); //col count
        for (auto& r  : t.second->table_data){ //rows
            for (auto& cell  : r){ //columns
                buf.write_zero_terminated_string(cell);
            }
        }
    }

    buf.write<uint32>(bo->jobs.size());
    for (auto& j : bo->jobs){
        buf.write_zero_terminated_string(j.first);
        buf.write<uint32>(j.second->job_code.size());
        buf.write(j.second->job_code.data(), j.second->job_code.size());
    }

    return true;
}

bmwBESTObject *DeserializeBESTObject(bmwMemoryBuffer &buf) {
    bmwBESTObject * bo =  new bmwBESTObject();
    bo->rev = buf.read<bmwBESTRevision>();
    bo->version = buf.read<bmwBESTVerison>();
    bo->last_changed = buf.read_zero_terminated_string();
    bo->filename = buf.read_zero_terminated_string();
    uint32 desc_size = buf.read<uint32>();
    for (uint32 i = 0; i < desc_size; i++){
        std::string desc1 = buf.read_zero_terminated_string();
        std::string desc2 = buf.read_zero_terminated_string();
        bo->desc[desc1] = desc2;
    }
    uint32 desc_jobs_size = buf.read<uint32>();
    for (uint32 i = 0; i < desc_jobs_size; i++){
        std::pair<std::string, bmwBESTDescriptionEntry*> p;
        p.first = buf.read_zero_terminated_string();
        p.second = new bmwBESTDescriptionEntry();
        p.second->desc_name = buf.read_zero_terminated_string();
        p.second->desc_comment = buf.read_zero_terminated_string();
        uint32 desc_jobs_lines_size = buf.read<uint32>();
        for (uint32 il = 0; il < desc_jobs_lines_size; il++){
            std::pair<bmwBESTDescriptionEntryType, std::string> dl;
            dl.first = buf.read<bmwBESTDescriptionEntryType>();
            dl.second = buf.read_zero_terminated_string();
            p.second->desc_lines.push_back(dl);
        }
        bo->desc_jobs[p.first] = p.second;
    }

    uint32 tables_size = buf.read<uint32>();
    for (uint32 i = 0; i < tables_size; i++){
        bmwBESTTable* table = new bmwBESTTable();
        table->table_name = buf.read_zero_terminated_string();
        uint32 table_head_size = buf.read<uint32>();
        for (uint32 it = 0; it < table_head_size; it++){
            table->table_head.push_back(buf.read_zero_terminated_string());
        }
        uint32 row_count = buf.read<uint32>();
        uint32 col_count = buf.read<uint32>();
        for (uint32 ir = 0; ir < row_count; ir++){
            std::vector<std::string> row;
            for (uint32 ic = 0; ic < col_count; ic++){
                row.push_back(buf.read_zero_terminated_string());
            }
            table->table_data.push_back(row);
        }
        bo->tables[table->table_name] = table;
    }

    uint32 jobs_size = buf.read<uint32>();
    for (uint32 i = 0; i < jobs_size; i++){
        bmwBESTJob* job = new bmwBESTJob();
        job->name = buf.read_zero_terminated_string();
        job->name = bmwStringHelper::lowercase(job->name);
        uint32 job_len = buf.read<uint32>();
        job->job_code.resize(job_len);
        buf.read(job->job_code.data(), job_len);
        bo->jobs[job->name] = job;
    }
    return bo;
}
