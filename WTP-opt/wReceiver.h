#ifndef wReceiver_h
#define wReceiver_h

#include <fstream>
#include <vector>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>

#include "../starter_files/PacketHeader.h"

#define BUFLEN 1472
#define DATALEN 1456

struct recvPacket{
    int seqNum;
    char data[1456];
    unsigned int length;
};


class wReceiver {

    public:
        bool SetupSocket();
        bool Start_Listening();
        void Connection_Procedure();
        void Setup_Output_Dir();
        void Log_Recvied_Data();
        int get_us_diff(struct timeval&, struct timeval&);

        wReceiver(int port_num_,  unsigned int window_size_, std::string output_dir_, std::ofstream* log_file_);
        ~wReceiver();


    private:
        int sock;
        int recv_len;

        // For Timing
        int sender_window_start_idx;
        int sender_window_end_idx;

        int port_num;
        unsigned int window_size;
        std::string output_dir;
        std::ofstream* log_file;
        char buf[BUFLEN];
        socklen_t slen;
};



#endif /* wReceiver_h */
