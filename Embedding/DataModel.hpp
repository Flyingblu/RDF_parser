#pragma once
#include "Import.hpp"
#include "ModelConfig.hpp"

class DataModel
{
public:
	set<pair<pair<unsigned int, unsigned int>, unsigned int> >		check_data_train;
	set<pair<pair<unsigned int, unsigned int>, unsigned int> >		check_data_all;

public:
	vector<pair<pair<unsigned int, unsigned int>, unsigned int> >	data_train;
	vector<pair<pair<unsigned int, unsigned int>, unsigned int> >	data_test_true;

public:
	set<unsigned int>			set_entity;
	set<unsigned int>			set_relation;

public:
	vector<char>	relation_type;

public:
	vector<double>		relation_tph;
	vector<double>		relation_hpt;

public:
	DataModel(const Dataset& dataset)
	{
		// TODO: these two maps seem to be huge, see if we can optimize it. 
		map<int, map<int, vector<int> > > rel_heads;
		map<int, map<int, vector<int> > > rel_tails;
		load_training(dataset.base_dir + dataset.training, rel_heads, rel_tails);
		relation_hpt.resize(set_relation.size());
		relation_tph.resize(set_relation.size());
		for(auto i=0; i!=set_relation.size(); ++i)
		{
			double sum = 0;
			double total = 0;
			for(auto ds=rel_heads[i].begin(); ds!=rel_heads[i].end(); ++ds)
			{
				++ sum;
				total += ds->second.size();
			}
			relation_tph[i] = total / sum;
		}
		for(auto i=0; i!=set_relation.size(); ++i)
		{
			double sum = 0;
			double total = 0;
			for(auto ds=rel_tails[i].begin(); ds!=rel_tails[i].end(); ++ds)
			{
				++ sum;
				total += ds->second.size();
			}
			relation_hpt[i] = total / sum;
		}

		load_testing(dataset.base_dir + dataset.testing, data_test_true);

		double threshold = 1.5;
		relation_type.resize(set_relation.size());

 		for(auto i=0; i<set_relation.size(); ++i)
		{
			if (relation_tph[i]<threshold && relation_hpt[i]<threshold)
			{
				relation_type[i] = 1;
			}
			else if (relation_hpt[i] <threshold && relation_tph[i] >= threshold)
			{
				relation_type[i] = 2;
			}
			else if (relation_hpt[i] >=threshold && relation_tph[i] < threshold)
			{
				relation_type[i] = 3;
			}
			else
			{
				relation_type[i] = 4;
			}
		}
	}

	void load_training(const string& file_path,
		map<int, map<int, vector<int> > >& rel_heads,
		map<int, map<int, vector<int> > >& rel_tails)
	{
		ifstream triple_file(file_path, ios_base::binary);

		size_t triple_size;
		triple_file.read((char*)& triple_size, sizeof(size_t));
		ProgressBar prog_bar("Deserializing binary file to triples:", triple_size, log_path);
		prog_bar.progress_begin();

		for (prog_bar.progress = 0; prog_bar.progress < triple_size && triple_file; ++prog_bar.progress) {

			unsigned int tri_arr[3];
			triple_file.read((char*)tri_arr, sizeof(unsigned int) * 3);

			data_train.push_back(make_pair(make_pair(tri_arr[0], tri_arr[2]),tri_arr[1]));
			check_data_train.insert(make_pair(make_pair(tri_arr[0], tri_arr[2]), tri_arr[1]));
			check_data_all.insert(make_pair(make_pair(tri_arr[0], tri_arr[2]), tri_arr[1]));

			set_entity.insert(tri_arr[0]);
			set_entity.insert(tri_arr[2]);
			set_relation.insert(tri_arr[1]);

			rel_heads[tri_arr[1]][tri_arr[0]].push_back(tri_arr[2]);
			rel_tails[tri_arr[1]][tri_arr[2]].push_back(tri_arr[0]);
		}
		triple_file.close();
		prog_bar.progress_end();

		if (prog_bar.progress != triple_size) {
			cerr << "triple_deserialize: Something wrong in binary file reading. " << endl;
		}
	}

	void load_testing (const string& file_path,
		vector<pair<pair<int, int>, int>>& vin_true)
	{
		ifstream triple_file(file_path, ios_base::binary);

		size_t triple_size;
		triple_file.read((char*)& triple_size, sizeof(size_t));
		ProgressBar prog_bar("Deserializing binary file to triples:", triple_size, log_path);
		prog_bar.progress_begin();

		for (prog_bar.progress = 0; prog_bar.progress < triple_size && triple_file; ++prog_bar.progress) {

			unsigned int tri_arr[3];
			triple_file.read((char*)tri_arr, sizeof(unsigned int) * 3);

			vin_true.push_back(make_pair(make_pair(tri_arr[0], tri_arr[2]), tri_arr[1]));
			check_data_all.insert(make_pair(make_pair(tri_arr[0], tri_arr[2]), tri_arr[1]));

			set_entity.insert(tri_arr[0]);
			set_entity.insert(tri_arr[2]);
			set_relation.insert(tri_arr[1]);
		}
		triple_file.close();
		prog_bar.progress_end();

		if (prog_bar.progress != triple_size) {
			cerr << "triple_deserialize: Something wrong in binary file reading. " << endl;
		}
	}

	void sample_false_triplet(	
		const pair<pair<int,int>,int>& origin,
		pair<pair<int,int>,int>& triplet) const
	{

		double prob = relation_hpt[origin.second]/(relation_hpt[origin.second] + relation_tph[origin.second]);

		triplet = origin;
		while(true)
		{
			if(rand()%1000 < 1000 * prob)
			{
				triplet.first.second = rand()%set_entity.size();
			}
			else
			{
				triplet.first.first = rand()%set_entity.size();
			}

			if (check_data_train.find(triplet) == check_data_train.end())
				return;
		}
	}
};