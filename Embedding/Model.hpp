#pragma once
#include "Import.hpp"
#include "ModelConfig.hpp"
#include "DataModel.hpp"
#include <boost/progress.hpp>

using namespace std;
using namespace arma;

class Model
{
public:
	const DataModel &data_model;
	const TaskType task_type;
	const bool be_deleted_data_model;

public:
	ModelLogging &logging;

public:
	int epos;

public:
	Model(const Dataset &dataset,
		  const TaskType &task_type,
		  const string &logging_base_path, 
		  bool do_load_testing)
		: data_model(*(new DataModel(dataset, do_load_testing))), task_type(task_type),
		  logging(*(new ModelLogging(logging_base_path))),
		  be_deleted_data_model(true)
	{
		epos = 0;
		std::cout << "Ready" << endl;

		logging.record() << "\t[Dataset]\t" << dataset.name;
		logging.record() << TaskTypeName(task_type);
	}

public:
	virtual double prob_triplets(const pair<pair<unsigned int, unsigned int>, unsigned int> &triplet) = 0;
	virtual void train_triplet(const pair<pair<unsigned int, unsigned int>, unsigned int> &triplet) = 0;

public:
	virtual void train(bool last_time = false)
	{
		++epos;

#pragma omp parallel for
		for (auto i = data_model.data_train.begin(); i != data_model.data_train.end(); ++i)
		{
			train_triplet(*i);
		}
	}

	void run(int total_epos)
	{
		logging.record() << "\t[Epos]\t" << total_epos;

		--total_epos;
		boost::progress_display cons_bar(total_epos);
		while (total_epos-- > 0)
		{
			++cons_bar;
			train();
		}

		train(true);
	}

public:

	double		best_link_mean;
	double		best_link_hitatten;
	double		best_link_fmean;
	double		best_link_fhitatten;

	void test_link_prediction(int hit_rank = 10, const int part = 0)
	{
		double mean = 0;
		double hits = 0;
		double fmean = 0;
		double fhits = 0;
		double rmrr = 0;
		double fmrr = 0;
		double total = data_model.data_test_true.size();

		double arr_mean[20] = {0};
		double arr_total[5] = {0};

		for (auto i = data_model.data_test_true.begin(); i != data_model.data_test_true.end(); ++i)
		{
			++arr_total[data_model.relation_type[i->second]];
		}

		int cnt = 0;

		boost::progress_display cons_bar(data_model.data_test_true.size() / 100);

#pragma omp parallel for
		for (auto i = data_model.data_test_true.begin(); i != data_model.data_test_true.end(); ++i)
		{
			++cnt;
			if (cnt % 100 == 0)
			{
				++cons_bar;
			}

			pair<pair<int, int>, int> t = *i;
			int frmean = 0;
			int rmean = 0;
			double score_i = prob_triplets(*i);

			if (task_type == LinkPredictionRelation || part == 2)
			{
				for (auto j = 0; j != data_model.set_relation.size(); ++j)
				{
					t.second = j;

					if (score_i >= prob_triplets(t))
						continue;

					++rmean;

					if (data_model.check_data_all.find(t) == data_model.check_data_all.end())
						++frmean;
				}
			}
			else
			{
				for (auto j = 0; j != data_model.set_entity.size(); ++j)
				{
					if (task_type == LinkPredictionHead || part == 1)
						t.first.first = j;
					else
						t.first.second = j;

					if (score_i >= prob_triplets(t))
						continue;

					++rmean;

					if (data_model.check_data_all.find(t) == data_model.check_data_all.end())
					{
						++frmean;
						//if (frmean > hit_rank)
						//	break;
					}
				}
			}

#pragma omp critical
			{
				if (frmean < hit_rank)
					++arr_mean[data_model.relation_type[i->second]];

				mean += rmean;
				fmean += frmean;
				rmrr += 1.0 / (rmean + 1);
				fmrr += 1.0 / (frmean + 1);

				if (rmean < hit_rank)
					++hits;
				if (frmean < hit_rank)
					++fhits;
			}
		}

		std::cout << endl;
		for (auto i = 1; i <= 4; ++i)
		{
			std::cout << i << ':' << arr_mean[i] / arr_total[i] << endl;
			logging.record() << i << ':' << arr_mean[i] / arr_total[i];
		}
		logging.record();

		best_link_mean = min(best_link_mean, mean / total);
		best_link_hitatten = max(best_link_hitatten, hits / total);
		best_link_fmean = min(best_link_fmean, fmean / total);
		best_link_fhitatten = max(best_link_fhitatten, fhits / total);

		std::cout << "Raw.BestMEANS = " << best_link_mean << endl;
		std::cout << "Raw.BestMRR = " << rmrr / total << endl;
		std::cout << "Raw.BestHITS = " << best_link_hitatten << endl;
		logging.record() << "Raw.BestMEANS = " << best_link_mean;
		logging.record() << "Raw.BestMRR = " << rmrr / total;
		logging.record() << "Raw.BestHITS = " << best_link_hitatten;

		std::cout << "Filter.BestMEANS = " << best_link_fmean << endl;
		std::cout << "Filter.BestMRR= " << fmrr / total << endl;
		std::cout << "Filter.BestHITS = " << best_link_fhitatten << endl;
		logging.record() << "Filter.BestMEANS = " << best_link_fmean;
		logging.record() << "Filter.BestMRR= " << fmrr / total;
		logging.record() << "Filter.BestHITS = " << best_link_fhitatten;

		std::cout.flush();
	}

	virtual void draw(const string &filename, const int radius, const int id_relation) const
	{
		return;
	}

	virtual void draw(const string &filename, const int radius,
					  const int id_head, const int id_relation)
	{
		return;
	}

	virtual void report(const string &filename) const
	{
		return;
	}

public:
	~Model()
	{
		logging.record();
		if (be_deleted_data_model)
		{
			delete &data_model;
			delete &logging;
		}
	}

public:
	int count_entity() const
	{
		return data_model.set_entity.size();
	}

	int count_relation() const
	{
		return data_model.set_relation.size();
	}

	const DataModel &get_data_model() const
	{
		return data_model;
	}

public:
	virtual void save(const string &filename)
	{
		cout << "BAD";
	}

	virtual void load(const string &filename)
	{
		cout << "BAD";
	}

	virtual vec entity_representation(unsigned int entity_id) const
	{
		cout << "BAD";
	}

	virtual vec relation_representation(unsigned int relation_id) const
	{
		cout << "BAD";
	}
};