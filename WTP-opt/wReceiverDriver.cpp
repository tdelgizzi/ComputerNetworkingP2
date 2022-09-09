#include "wReceiver.h"

#include <iostream>
#include <fstream>
#include <string>

using namespace std;

int main(int argc, char *argv[]){

    // args
    int port_num_in = stoi(argv[1]);
    unsigned int window_size_in = stoi(argv[2]);
    string output_dir_in = string(argv[3]);
//    if (output_dir_in.front() == '/') {
//        output_dir_in.erase(0, 1);
//    }
    ofstream log_file (argv[4],ios::trunc);
    // init receiver
    wReceiver receiver(port_num_in, window_size_in, output_dir_in, &log_file);
    // init receiver socket
    if (!receiver.SetupSocket()) {
        return -1;
    }
    // Start listening for connections
    receiver.Start_Listening();
    return 0;

}
