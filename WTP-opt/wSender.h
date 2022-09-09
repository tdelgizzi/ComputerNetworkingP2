#ifndef WSENDER
#define WSENDER

#include <fstream>
#include <vector>
#include <string>
#include "../starter_files/PacketHeader.h"

struct PacketInfo{
    bool acked;
    timeval last_send_time;
};

class wSender {

  public:
    bool SetupSocket();
    void InitContainer();
    void Start();
    wSender(std::string receiver_ip, int receiver_port, unsigned int window_size, std::ifstream* input_file, std::ofstream* log_file);
    int get_us_diff(struct timeval&, struct timeval&);
    ~wSender();

  private:
    int socket_;
    // [start, end)
    int sender_window_start_idx_;
    int sender_window_end_idx_;
    std::vector<PacketHeader> header_container_;
    std::vector<std::string> buffer_container_;
    std::vector<PacketInfo> info_container_;



    std::string receiver_ip_;
    int receiver_port_;
    int window_size_;
    std::ifstream* input_file_;
    std::ofstream* log_file_;
};

#endif
