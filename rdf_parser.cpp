//
//  rdf_parser.cpp
//  Trival_RDF_parser
//
//  Created by 王嘉睿 on 2019/5/9.
//  Copyright © 2019 Jerry's World. All rights reserved.
//

#include "rdf_parser.hpp"

using namespace std;

unsigned int RDFParser::put_to_map(unordered_map<string, unsigned int>& target_map, string& key, unsigned int& id_pos) {

    auto i = target_map.find(key);
    if(i == target_map.end()) {

        target_map[key] = id_pos;
        id_pos += 1;
        return id_pos - 1;
    }
    return i->second;
}

void RDFParser::triple_parser(string& triple) {
    int state = 0;
    int f_state = 0;
    unsigned int triple_arr[3];
    auto add_to_map = [=](string::iterator start, string::iterator end, int & s, unsigned int * tri) -> void {
        string tmp(start, end);
        switch (s)
        {
        case 0:
            tri[s] = this->put_to_map(this->entities, tmp, this->ent_pos);
            break;
        
        case 1:
            tri[s] = this->put_to_map(this->properties, tmp, this->prop_pos);
            break;

        case 2:
            tri[s] = this->put_to_map(this->entities, tmp, this->ent_pos);
            break;

        default:
            break;

        }
        ++s;
        return;
    };
    bool is_blank = false;
    auto start = triple.begin();

    for(auto i = triple.begin(); i != triple.end(); i++) {

        if (f_state == 0) {

            if (*i == '<') {

                f_state = 1;
                start = i;

            } else if (*i == '"') {

                f_state = 2;
                start = i;

            }
        } else if (f_state == 1) {

            if (*i == ' ' || *i == '\t') {

                f_state = 0;
                add_to_map(start, i, state, triple_arr);

                if (state == 3) {
                    this->triples.push_back(make_tuple(triple_arr[0], triple_arr[1], triple_arr[2]));
                    break;
                }

            }
        } else if (f_state == 2) {

            if (*i == '"') {

                f_state = 1;

            } else if (*i == '\\') {

                f_state = 3;

            }
        } else if (f_state == 3) {

            f_state = 2;

        }
    }
}

bool RDFParser::batch_parser(unsigned int batch_size, ProgressBar& prog_bar) {
    string line;
    bool end = true;

    for(unsigned int i = 0; i < batch_size; ++i) {

        if(getline(*(this->rdf_file), line)) {
            this->triple_parser(line);

        } else {
            end = false;
            break;
        }

        if(i % 100 == 0) {
            prog_bar.progress_increment(100);
        }
    }
    return end;
}

void RDFParser::parse(unsigned int lines, unsigned int batch_size, bool save_file) {

    string line;
    ProgressBar prog_bar("Triples parsed:");

    if(lines == -1) {
        bool remain = true;
        
        while(remain) {
            
            remain = this->batch_parser(batch_size, prog_bar);
            if(save_file) {
                MapSerializer::map_serialize(this->entities, this->save_path + "entities.data");
                MapSerializer::map_serialize(this->properties, this->save_path + "properties.data");
                MapSerializer::triple_serialize(this->triples, this->save_path + "triples.data");
            }
        }

    } else {
        unsigned int pos = 0;
        bool end = true;

        while(end) {

            if(pos + batch_size < lines) {

                end = this->batch_parser(batch_size, prog_bar);
                pos += batch_size;

            } else {

                end = false;
                this->batch_parser(lines - pos, prog_bar);
            }
            if(save_file) {
                MapSerializer::map_serialize(this->entities, this->save_path + "entities.data");
                MapSerializer::map_serialize(this->properties, this->save_path + "properties.data");
                MapSerializer::triple_serialize(this->triples, this->save_path + "triples.data");
            }
        }
    }
    prog_bar.progress_end();
}

void RDFParser::to_json(string path) {

    cout << "Saving to JSON..." << endl;

    ofstream json_file(path);
    nlohmann::json j;
    j["entities"] = this->entities;
    j["properties"] = this->properties;
    j["triples"] = this->triples;
    json_file << setw(4) << j;
    json_file.close();
}

void RDFParser::to_text(string path)
{
    MapSerializer::map_to_text(this->entities, path + "entities.text");
    MapSerializer::map_to_text(this->properties, path + "properties.text");
}

void RDFParser::clear_data() {
    this->entities.clear();
    this->properties.clear();
    this->triples.clear();
}

void RDFParser::retrivial() {
    MapSerializer::map_deserialize(this->entities, this->save_path + "entities.data");
    MapSerializer::map_deserialize(this->properties, this->save_path + "properties.data");
    MapSerializer::triple_deserialize(this->triples, this->save_path + "triples.data");
}
