#ifndef clusterization_hpp
#define clusterization_hpp

#include <vector>
#include <string>
#include <fstream>
#include <iostream>
using namespace std;
class cluster
{
    public:
        cluster(string read_path, string save_path, unsigned int reserve_num): reader(new ifstream(read_path, ios::binary)), save_path(save_path), cunt(reserve_num, 1){
            cout << "Initializing ... " << endl;
            us.resize(reserve_num);
            for (int i = 0; i < reserve_num; i++)
            {
                us[i] = i;
            }
        };
        ~cluster()
        {
            delete this->reader;
        };
        void clusterizing();    
        void logging();
        vector<unsigned int> getunionset();
        vector<unsigned int> getuscount();
        

    private:
        ifstream* reader;
        vector<unsigned int> us;
        vector<unsigned int> cunt;
        string save_path; 
        unsigned int find(unsigned int id);
        void join(unsigned int idl, unsigned int idr);
};
#endif