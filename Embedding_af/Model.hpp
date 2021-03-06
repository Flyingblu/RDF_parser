#pragma once
#include "Import.hpp"
#include "ModelConfig.hpp"
#include "DataModel.hpp"
#include "../RDF_parser/progress_bar.hpp"
#include <thread>
#include <mutex>

using namespace std;
using namespace arma;

class Model
{
public:
	vector<DataModel *> data_models;
	unsigned int current_data_model;
	const DataModel *data_model;
	const DataModel *test_data_model = nullptr;
	mutex data_models_mut;
	const TaskType task_type;

public:
	ModelLogging &logging;

public:
	int epos;

public:
	const string save_path;

public:
	Model(const TaskType &task_type,
		  const string &logging_base_path,
		  const string& save_path)
		: task_type(task_type),
		  logging(*(new ModelLogging(logging_base_path))),
		  save_path(save_path)
	{
		epos = 0;
		std::cout << "Ready" << endl;
		logging.record() << TaskTypeName(task_type);
	}

public:
	void load_datasets(vector<Dataset *> &datasets, size_t entity_size, size_t relation_size, int start, int range)
	{
		vector<thread *> threads;
		for (int i = start; i < range + start; ++i)
		{
			threads.push_back(new thread(&Model::load_dataset, this, datasets[i], entity_size, relation_size));
		}
		for (auto a_thread : threads)
		{
			a_thread->join();
			delete a_thread;
		}
	}

	void load_dataset(Dataset *dataset, size_t entity_size, size_t relation_size)
	{
		auto data = new DataModel(dataset, false, entity_size, relation_size);
		data_models_mut.lock();
		data_models.push_back(data);
		data_models_mut.unlock();
	}

	bool switch_dataset()
	{
		data_model = data_models[current_data_model];
		++current_data_model;
		return current_data_model >= data_models.size() ? false : true;
	}

	void zero_dataset_cur()
	{
		current_data_model = 0;
	}

	void load_test_dataset(Dataset *dataset, size_t entity_size, size_t relation_size)
	{
		test_data_model = new DataModel(dataset, true, entity_size, relation_size);
	}

public:
	//virtual double prob_triplets(const pair<pair<unsigned int, unsigned int>, unsigned int> &triplet) = 0;
	virtual af::array prob_triplets(const af::array& mat_triplet) { return af::constant(1, 1, 1); };
	virtual void train_triplet(vector<unsigned int>& head_batch, vector<unsigned int>& relation_batch, vector<unsigned int>& tail_batch, vector<size_t> &index_batch) = 0;

public:
	virtual void train_batch(size_t start, size_t length)
	{
		size_t end = start + length;
		vector<unsigned int> head_batch(length);
		vector<unsigned int> relation_batch(length);
		vector<unsigned int> tail_batch(length);
		vector<size_t> index_batch(length);
		for (size_t i = start; i < end; ++i)
		{
			head_batch[i - start] = data_model->data_train[i].first.first;
			relation_batch[i - start] = data_model->data_train[i].second;
			tail_batch[i - start] = data_model->data_train[i].first.second;
			index_batch[i - start] = i;
		}
		train_triplet(head_batch, relation_batch, tail_batch, index_batch);
	}

	virtual void train(int parallel_thread, vector<Dataset *> &dataset)
	{
		
		//size_t num_each_thread = data_model->data_train.size() / parallel_thread;
		//size_t num_cores = 3584;
		size_t num_cores = data_model->data_train.size();
		size_t loop = data_model->data_train.size() / num_cores;
		//vector<thread *> threads(parallel_thread);
		for (auto i = 0; i < loop; ++i)
		{
			if (i == loop - 1)
			{
				train_batch(i * num_cores, data_model->data_train.size() - i * num_cores);
				//threads[i] = new thread(&Model::train_batch, this, i * num_cores, data_model->data_train.size() - i * num_cores);
				continue;
			}
			train_batch(i * num_cores, num_cores);
			//threads[i] = new thread(&Model::train_batch, this, i * num_cores, num_cores);
		}
		/*
		for (auto i = 0; i < parallel_thread; ++i)
		{
			if (i == parallel_thread - 1)
			{
				threads[i] = new thread(&Model::train_batch, this, i * num_each_thread, data_model->data_train.size() - i * num_each_thread);
				continue;
			}
			threads[i] = new thread(&Model::train_batch, this, i * num_each_thread, num_each_thread);
		}
		
		
		for (auto i = 0; i < parallel_thread; ++i)
		{
			threads[i]->join();
			delete threads[i];
		}
		*/
	}

	void run(int total_epos, int parallel_thread, vector<Dataset *> &dataset)
	{
		logging.record() << "\t[Epos]\t" << total_epos;
		
		--total_epos;
		if (epos != 0)
		{
			total_epos -= epos;
		}
		cout << endl;
		cout << "start training from Round : " << epos << endl;
		cout << "round left : " << total_epos + 1 << endl;
		ProgressBar prog_bar("Training", total_epos);
		prog_bar.progress_begin();
		//af::timer start = af::timer::start();
		while (total_epos-- > 0)
		{
			af::timer start = af::timer::start();
			++prog_bar.progress;
			cout << "Round : " << prog_bar.progress << endl;
			if (!(prog_bar.progress % 100))
			{
				cout << prog_bar.progress << "round saveing " << endl;
				epos += prog_bar.progress;
				save(save_path);
			}
			train(parallel_thread, dataset);
			cout << "Time : " << af::timer::stop() << endl;
		}
		train(parallel_thread, dataset);
		prog_bar.progress_end();
	}

public:
	double best_link_mean;
	double best_link_hitatten;
	double best_link_fmean;
	double best_link_fhitatten;

	void test_batch(size_t start, size_t length, vector<double> &result, int hit_rank, const int part, long long &progress, mutex *progress_mtx)
	{

		size_t end = start + length;
		vector<unsigned int> test_head_batch(length);
		vector<unsigned int> test_relation_batch(length);
		vector<unsigned int> test_tail_batch(length);
		for (size_t i = start; i < end; ++i)
		{
			pair<pair<int, int>, int> t = test_data_model->data_test_true[i];
			test_head_batch[i] = t.first.first;
			test_relation_batch[i] = t.second;
			test_tail_batch[i] = t.first.second;
		}

		af::array mat_test_triplet(3, length, af::dtype::u32);
		mat_test_triplet(0, af::span) = af::array(1, length, test_head_batch.data());
		mat_test_triplet(1, af::span) = af::array(1, length, test_relation_batch.data());
		mat_test_triplet(2, af::span) = af::array(1, length, test_tail_batch.data());
		test_head_batch.clear();
		test_relation_batch.clear();
		test_tail_batch.clear();
		af::array score = prob_triplets(mat_test_triplet);
		
		
		
		for (size_t i = start; i < end; ++i)
		{

			af::timer start = af::timer::start();
			
			int rmean = 0;
			if (task_type == LinkPredictionRelation || part == 2)
			{
				af::array score_i = af::tile(score(0, i), 1, test_data_model->relation_size);
				af::array mat_test_triplet_t(3, test_data_model->relation_size, af::dtype::u32);
				mat_test_triplet_t(0, af::span) = af::tile(mat_test_triplet(0, i), 1, test_data_model->relation_size);
				mat_test_triplet_t(1, af::span) = af::range(af::dim4(1, test_data_model->relation_size), 1);
				mat_test_triplet_t(2, af::span) = af::tile(mat_test_triplet(2, i), 1, test_data_model->relation_size);
				af::array cond = score_i >= prob_triplets(mat_test_triplet_t);
				cond = cond.as(af::dtype::s32);
				int* cond_h = cond.host<int>();
				for (auto j = 0; j != test_data_model->relation_size; ++j)
				{

					if (!cond_h[j])
					{
						++rmean;
					}
				}
				af::freeHost(cond_h);
			}
			else
			{
				af::array score_i = af::tile(score(0, i), 1, test_data_model->entity_size);
				af::array mat_test_triplet_t(3, test_data_model->entity_size, af::dtype::u32);
				if (task_type == LinkPredictionHead || part == 1)
				{

					mat_test_triplet_t(0, af::span) = af::range(af::dim4(1, test_data_model->entity_size), 1);
					mat_test_triplet_t(1, af::span) = af::tile(mat_test_triplet(1, i), 1, test_data_model->entity_size);
					mat_test_triplet_t(2, af::span) = af::tile(mat_test_triplet(2, i), 1, test_data_model->entity_size);
				}
				else 
				{
					mat_test_triplet_t(0, af::span) = af::tile(mat_test_triplet(0, i), 1, test_data_model->entity_size);
					mat_test_triplet_t(1, af::span) = af::tile(mat_test_triplet(1, i), 1, test_data_model->entity_size);
					mat_test_triplet_t(2, af::span) = af::range(af::dim4(1, test_data_model->entity_size), 1);
				}
				af::array cond = score_i >= prob_triplets(mat_test_triplet_t);
				cond = cond.as(af::dtype::s32);
				int* cond_h = cond.host<int>();
				for (auto j = 0; j != test_data_model->entity_size; ++j)
				{
					if (!cond_h[j])
					{
						++rmean;
					}
				}
				af::freeHost(cond_h);
			}

			result[0] += rmean;
			result[4] += 1.0 / (rmean + 1);

			if (rmean < hit_rank)
				++result[1];

			if (!(i % 100))
			{
				progress_mtx->lock();
				progress += 100;
				progress_mtx->unlock();

			}
			cout << "Time : " << af::timer::stop() << endl;
		}
	}

	void test_link_prediction(int hit_rank = 10, const int part = 0, int parallel_thread = 1)
	{
		best_link_mean = 1e10;
		best_link_hitatten = 0;
		best_link_fmean = 1e10;
		best_link_fhitatten = 0;
		
		double mean = 0;
		double hits = 0;
		double fmean = 0;
		double fhits = 0;
		double rmrr = 0;
		double fmrr = 0;
		double total = test_data_model->data_test_true.size();
		std::cout << "data_test_true.size : " << test_data_model->data_test_true.size() << std::endl;

		double arr_mean[20] = {0};
		double arr_total[5] = {0};

		//size_t num_cores = test_data_model->data_test_true.size();
		//parallel_thread = test_data_model->data_test_true.size() / num_cores;

		
		vector<thread *> threads(parallel_thread);
		size_t num_each_thread = test_data_model->data_test_true.size() / parallel_thread;
		vector<vector<double>> thread_data(parallel_thread, vector<double>(26));
		for (auto i = test_data_model->data_test_true.begin(); i != test_data_model->data_test_true.end(); ++i)
		{
			++arr_total[test_data_model->relation_type[i->second]];
		}
		
		ProgressBar prog_bar("Testing link prediction", test_data_model->data_test_true.size());
		mutex* progress_mtx = new mutex();
		prog_bar.progress_begin();
		for (auto i = 0; i < parallel_thread; ++i)
		{
			
			if (i = parallel_thread - 1)
			{
				test_batch(i * num_each_thread, test_data_model->data_test_true.size() - i * num_each_thread, thread_data[i], hit_rank, part, prog_bar.progress, progress_mtx);
			}
			test_batch(i * num_each_thread, num_each_thread, thread_data[i], hit_rank, part, prog_bar.progress, progress_mtx);
			
		}
		/*
		mutex *progress_mtx = new mutex();
		prog_bar.progress_begin();
		
		for (auto i = 0; i < parallel_thread; ++i)
		{
			if (i == parallel_thread - 1)
			{
				threads[i] = new thread(&Model::test_batch, this, i * num_each_thread, test_data_model->data_test_true.size() - i * num_each_thread, ref(thread_data[i]), hit_rank, part, ref(prog_bar.progress), progress_mtx);
				continue;
			}
			threads[i] = new thread(&Model::test_batch, this, i * num_each_thread, num_each_thread, ref(thread_data[i]), hit_rank, part, ref(prog_bar.progress), progress_mtx);
		}

		for (auto i = 0; i < parallel_thread; ++i)
		{
			threads[i]->join();
			delete threads[i];
		}
		*/
		for (auto i = 0; i < parallel_thread; ++i)
		{
			mean += thread_data[i][0];
			hits += thread_data[i][1];
			fmean += thread_data[i][2];
			fhits += thread_data[i][3];
			rmrr += thread_data[i][4];
			fmrr += thread_data[i][5];
			for (auto j = 0; j < 20; ++j)
			{
				arr_mean[j] += thread_data[i][6 + j];
			}
		}
		
		prog_bar.progress_end();

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
		for (auto i : data_models)
		{
			delete i;
		}
		delete &logging;
		if (test_data_model != nullptr)
		{
			delete test_data_model;
		}
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

	virtual fvec entity_representation(unsigned int entity_id) const
	{
		cout << "BAD";
		fvec a;
		return a;
	}

	virtual fvec relation_representation(unsigned int relation_id) const
	{
		cout << "BAD";
		fvec a;
		return a;
	}
};