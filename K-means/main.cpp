#include <iostream>
#include <vector>
#include <fstream>
#include "k_means.hpp"
using namespace std;

int main()
{
    k_means k;
    k.load_id("PATH_OF_ID");
    k.load_table("PATH_OF_TABLE");
    return 0;
}
