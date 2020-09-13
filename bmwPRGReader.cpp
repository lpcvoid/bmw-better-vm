//
// Created by lpcvoid on 2020-02-06.
//

#include <cstring>
#include "bmwPRGReader.h"
#include "../common/bmwStringHelper.hpp"
#include "../common/bmwLogger.h"

bool ReadInfo(bmwFileBuffer* fb, bmwBESTObject* bo);
bool ReadJobDesc(bmwFileBuffer* fb, bmwBESTObject* bo);
bool ReadJobs(bmwFileBuffer* fb, bmwBESTObject* bo);
bool ReadTables(bmwFileBuffer* fb, bmwBESTObject* bo);
int32 ReadAndDecrypt(bmwFileBuffer* buf, uint8* dest, int32 count, int32 offset = INT32_MAX);

bmwBESTObject * ReadBESTObject(std::string filepath) {

    //some BEST ecus don't load, even with tool32:
    //EDIABAS Fehler 69 : BIP-0009: BEST VERSION ERROR  ---> zbe6.prg
    try {
        bmwFileBuffer buf(filepath, RAX_IO_FILE_MODE_READ);
        //some prgs are strangely malformed
        //examples : zbe6.prg; hud_01.prg
        //simple check to determine if we can read this prg
        //go to table offset 0x85, read address, go to address, check if bytes are 00 here
        //normally there would be at least 1 table here; in malformed, there's just a lot of 0x00
        buf.Seek(0x84);
        uint32 table_addr = buf.Read<uint32>();
        buf.Seek(table_addr);
        uint64 table_content = buf.Read<uint64>();
        if (table_content){
            bmwBESTObject *bo = new bmwBESTObject();
            bo->filename = bmwStringHelper::lowercase(buf.GetFileName(false));
            ReadInfo(&buf, bo);
            ReadJobDesc(&buf, bo);
            ReadJobs(&buf, bo);
            ReadTables(&buf, bo);
            return bo;
        } else {

            BMWLOG().error("ReadBESTObject :: Malformed prg file :" + buf.GetFileName(false));
            return nullptr;
        }


    } catch (std::exception e){
        return nullptr;
    }

}

bool ReadInfo(bmwFileBuffer *fb, bmwBESTObject *bo) {
    fb->Seek(0x94);
    int info_offset = fb->Read<uint32_t>();
    uint8 *infobuf = (uint8 *) malloc(255);
    ReadAndDecrypt(fb, (uint8 *) infobuf, 0x6C, info_offset);
    bmwBESTVerison v;
    memcpy(&bo->version, &infobuf[0], 3);
    memcpy(&bo->rev, &infobuf[3], 4);
    std::string lc(&infobuf[0x48], &infobuf[0x48] + 0x24);
    bo->last_changed = bmwStringHelper::trim(lc);

    free(infobuf);
    return true;
}

int32 ReadAndDecrypt(bmwFileBuffer *buf, uint8 *dest, int32 count, int32 offset) {
    if (!dest)
        return -1;
    if (offset != INT32_MAX)
        buf->Seek(offset);
    buf->ReadData(dest, count);
    for (int i = 0; i < count; i++)
        dest[i] ^= 0xF7;
    return count;
}

bool ReadJobDesc(bmwFileBuffer *fb, bmwBESTObject *bo) {

    fb->Seek(0x90);
    int info_offset = fb->Read<uint32_t>();
    fb->Seek(info_offset);
    int desc_size = fb->Read<uint32>();
    uint8 *infobuf = (uint8 *) malloc(desc_size);
    ReadAndDecrypt(fb, infobuf, desc_size);
    int recordoffset = 0;
    int lastoffset = 0;
    bmwBESTDescriptionEntry *current_desc = NULL;

    while (recordoffset < desc_size) {

        uint8 current_byte = infobuf[recordoffset];

        if (current_byte == '\n') {

            std::string dataset = std::string(&infobuf[0 + lastoffset], &infobuf[0 + recordoffset]);

            lastoffset = recordoffset;

            bmwStringHelper::replace(dataset, "\n", "");
            dataset = bmwStringHelper::trim(dataset);

            std::vector<std::string> pairs = bmwStringHelper::split(dataset, ":");

            if (pairs.size() == 2) {
                if (pairs[0] == "JOBNAME") {
                    if (current_desc)
                        bo->desc_jobs[current_desc->desc_name] = current_desc;

                    current_desc = new bmwBESTDescriptionEntry();
                    current_desc->desc_name = bmwStringHelper::lowercase(pairs[1]);

                }

                if (pairs[0] == "JOBCOMMENT") {
                    if (current_desc)
                        current_desc->desc_comment = pairs[1];


                }

                if (pairs[0] == "RESULT") {
                    if (current_desc)
                        current_desc->desc_lines.push_back({bmwBESTDescriptionEntryType_result, pairs[1]});

                }

                if (pairs[0] == "RESULTTYPE") {
                    if (current_desc)
                        current_desc->desc_lines.push_back({bmwBESTDescriptionEntryType_result_type, pairs[1]});

                }

                if (pairs[0] == "RESULTCOMMENT") {
                    if (current_desc)
                        current_desc->desc_lines.push_back({bmwBESTDescriptionEntryType_result_comment, pairs[1]});

                }

            }

        }
        recordoffset++;
    }

    free(infobuf);

    return true;
}

bool ReadJobs(bmwFileBuffer *fb, bmwBESTObject *bo) {
    fb->Seek(0x88);
    uint32 job_offset = fb->Read<uint32>();
    fb->Seek(job_offset);
    uint32 job_count = fb->Read<uint32>();

    uint8 *infobuf = (uint8 *) malloc(0x44);
    uint32 fspos = 0;
    for (int i = 0; i < job_count; i++) {
        bmwBESTJob *job = new bmwBESTJob();


        ReadAndDecrypt(fb, infobuf, 0x44);
        fspos = fb->GetPosition();


        int k = 0;
        while (1) {
            if (infobuf[k++] == 0) {
                job->name = std::string(&infobuf[0], &infobuf[k-1]);
                job->name = bmwStringHelper::trim(job->name);
                job->name = bmwStringHelper::lowercase(job->name);
                break;
            }
        }

        uint32 code_offset = *(uint32 *) &infobuf[0x40];

        fb->Seek(code_offset);

        uint8 eojeoj[4];

        ReadAndDecrypt(fb, &eojeoj[0], 4);
        uint32 c = 0;
        while (1) {

            if (*(uint32 *) &eojeoj == 1900573)
                break;

            eojeoj[0] = eojeoj[1];
            eojeoj[1] = eojeoj[2];
            eojeoj[2] = eojeoj[3];

            ReadAndDecrypt(fb, &eojeoj[3], 1);
            c++;
        }

        uint32 job_len = fb->GetPosition() - code_offset;
        job->job_code.resize(job_len);
        fb->Seek(code_offset);
        ReadAndDecrypt(fb, job->job_code.data(), job_len);

        bo->jobs[job->name] = job;

        fb->Seek(fspos); // set fs position back to job table entry. it needs to read next one.

    }

    free(infobuf);

    return true;


}

bool ReadTables(bmwFileBuffer *fb, bmwBESTObject *bo) {
    fb->Seek(0x84);
    int table_offset = fb->Read<uint32_t>();

    uint8 *tablebuffer = (uint8 *) malloc(0x50);
    uint8 *tableitembuffer = (uint8 *) malloc(1024);

    uint32 table_count = 0;

    ReadAndDecrypt(fb, (uint8 *) &table_count, sizeof(table_count), table_offset);

    for (int i = 0; i < table_count; i++) {

        bmwBESTTable *table = new bmwBESTTable();
        ReadAndDecrypt(fb, tablebuffer, 0x50);
        table->table_name = std::string(tablebuffer, tablebuffer + 0x40);
        table->table_name = bmwStringHelper::trim(table->table_name);
        table->table_name = bmwStringHelper::uppercase(table->table_name);

        uint32 table_column_offset = *(uint32 *) &tablebuffer[0x40];
        uint32 table_column_count = *(uint32 *) &tablebuffer[0x48];
        uint32 table_row_count = *(uint32 *) &tablebuffer[0x4C];
        uint32 savedpos = fb->GetPosition();

        fb->Seek(table_column_offset);

        for (int c = 0; c < table_column_count; c++) {
            int k = 0;
            do {
                ReadAndDecrypt(fb, &tableitembuffer[k++], 1);
            } while (tableitembuffer[k - 1] != 0);

            std::string head = std::string(&tableitembuffer[0], &tableitembuffer[k - 1]);
            head = bmwStringHelper::trim(head);
            table->table_head.push_back(head);

        }
        for (int trc = 0; trc < table_row_count; trc++) {

            table->table_data.push_back(std::vector<std::string>());

            for (int tcc = 0; tcc < table_column_count; tcc++) {

                int k = 0;
                do {
                    ReadAndDecrypt(fb, &tableitembuffer[k++], 1);
                } while (tableitembuffer[k - 1] != 0);


                if ((k - 1 < 0))
                    table->table_data[trc].push_back("");
                else{
                    std::string entry (&tableitembuffer[0], &tableitembuffer[k - 1]);
                    table->table_data[trc].push_back(bmwStringHelper::trim(entry));
                }



            }


        }

        bo->tables[table->table_name] = table;
        fb->Seek(savedpos);
    }

    free(tablebuffer);
    free(tableitembuffer);

    return false;
}
