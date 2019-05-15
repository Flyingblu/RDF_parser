//
//  progress_bar.cpp
//  Trival_RDF_parser
//
//  Created by 王嘉睿 on 2019/5/10.
//  Copyright © 2019 Jerry's World. All rights reserved.
//

#include "progress_bar.hpp"

void ProgressBar::progress_increment(long long progress) {

  if (this->ended) {
    cerr << "ProgressBar: progress already ended! " << endl;
    return;
  }

  string output = this->message + "\t";
  this->progress += progress;
  printf("%c[2K", 27);
  cout << '\r';
  if(this->graduation != -1) {

    double percentage = double(this->progress) / double(this->graduation);
    cout << setprecision(4) << output << percentage * 100 << '%' << '\r' << flush;

  } else {

    cout << output << this->progress << '\r' << flush;
  }
}

void ProgressBar::progress_end() {

  if (this->ended) {

      cerr << "ProgressBar: progress already ended! " << endl;
      return;
  }

  this->ended = true;
  cout << endl;

  auto end_time = std::chrono::system_clock::now();
  chrono::duration<double> elapsed_seconds = end_time-this->start_time;
  time_t end_time_t = std::chrono::system_clock::to_time_t(end_time);

  cout << "Finished at: " << ctime(&end_time_t);
  cout << "Elapsed time: " << elapsed_seconds.count() << 's' << endl;
}