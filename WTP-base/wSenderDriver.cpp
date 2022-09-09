#include "wSender.h"

#include <iostream>
#include <fstream>
#include <string>

using namespace std;

int main(int argc, char *argv[]){

  // args
  string receiver_ip = argv[1];
  int receiver_port = stoi(argv[2]);
  unsigned int window_size = stoi(argv[3]);
  ifstream input_file (argv[4]);
  ofstream log_file (argv[5],ios::trunc);
  // init sender
  wSender sender(receiver_ip, receiver_port, window_size, &input_file, &log_file);
  // init sender socket
  if (!sender.SetupSocket()) {
    return -1;
  }
  // transmission
  sender.InitContainer();
  sender.Start();
  return 0;

}
