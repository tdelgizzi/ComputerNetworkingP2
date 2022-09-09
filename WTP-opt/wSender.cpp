#include "wSender.h"
#include "../starter_files/PacketHeader.h"
#include "../starter_files/crc32.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>

#include <iostream>
#include <fstream>
#include <string>
#include <cerrno>
#include <chrono>

using namespace std;

wSender::wSender(string receiver_ip, int receiver_port, unsigned int window_size, ifstream* input_file, ofstream* log_file) :
receiver_ip_(receiver_ip), receiver_port_(receiver_port), window_size_(window_size), input_file_(input_file), log_file_(log_file) {}

wSender::~wSender() {
  if (socket_ != -1) {
    close(socket_);
  }
  if (input_file_->is_open()) {
    input_file_->close();
  }
  if (log_file_->is_open()) {
    log_file_->close();
  }
}

// set up UDP socket and bind
bool wSender::SetupSocket() {
  socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (socket_ == -1) {
    cerr << "Error: bad socket\n";
    return false;
  }

  int ok = 1;
  // to reuse ports
  if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &ok, sizeof(ok)) == -1) {
    cerr << "Error: bad setsockopt SO_REUSEADDR\n";
    return false;
  }

  // maximum receive block time 500 ms
  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 500000;
  if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
    cerr << "Error: bad setsockopt SO_RCVTIMEO\n";
    return false;
  }


  // struct sockaddr_in addr;
  // socklen_t length = (socklen_t)sizeof(addr);
  //
  // memset(&addr, 0, length);
  // addr.sin_family = AF_INET;
  // addr.sin_addr.s_addr = htonl(INADDR_ANY);
  // // server will occupy port 9000
  // addr.sin_port = htons(9000);
  //
  // if (bind(socket_, (const struct sockaddr *)&addr, length) == -1) {
  //   cerr << "Error: bad bind\n";
  //   return false;
  // }
  return true;
}

// initialize sender window start idx to 0
// initialize sender window end idx to window_size_ ( [start, end) )
// initialize the containers as vectors holding all packetheaders and bodies
void wSender::InitContainer() {
  sender_window_start_idx_ = 0;
  sender_window_end_idx_ = window_size_;

  // find the file length
  input_file_->seekg (0, input_file_->end);
  unsigned int remaining = input_file_->tellg();
  input_file_->seekg(0, input_file_->beg);
  unsigned int seqNum = 0;
  // max package body size = 1500 - 20 - 8 - 16 = 1456
  unsigned int max_package_size = 1456;
  while (remaining > 0) {
    // fill buffer container
    int cur_len = min(max_package_size, remaining);
    char buffer[cur_len];
    input_file_->read(buffer, cur_len);
    string buffer_s (buffer, cur_len);
    buffer_container_.push_back(buffer_s);

    // fill packetheader container
    struct PacketHeader cur_header;
    cur_header.type = 2;
    cur_header.seqNum = seqNum;
    cur_header.length = cur_len;
    cur_header.checksum = crc32(buffer, cur_len);
    header_container_.push_back(cur_header);

    // fill info container
    struct PacketInfo info;
    info.acked = false;
    memset(&(info.last_send_time), 0, sizeof(timeval));
    info_container_.push_back(info);

    seqNum += 1;
    remaining -= cur_len;
  }
  if ( sender_window_end_idx_ > header_container_.size()){
    sender_window_end_idx_ = header_container_.size();
  }
}

void wSender::Start() {
  // set up receiver info
  struct sockaddr_in receiver_info;
  memset((char*) &receiver_info, 0, sizeof(receiver_info));
  receiver_info.sin_family = AF_INET;
  receiver_info.sin_port = htons(receiver_port_);
  struct hostent* sp = gethostbyname(receiver_ip_.c_str());
  memcpy(&receiver_info.sin_addr, sp->h_addr, sp->h_length);
  socklen_t slen = sizeof(receiver_info);

  // set up the START packet
  struct PacketHeader start_header;
  start_header.type = 0;
  srand(time(0));
  start_header.seqNum = rand();
  while (start_header.seqNum < header_container_.size()) {
    start_header.seqNum = rand();
  }
  start_header.length = 0;
  start_header.checksum = 0;

  // send start packet
  // start action switch
  bool start_ack = false;
  // timestamp containers
  timeval start_time;
  timeval current_time;
  // buffer
  char buffer[sizeof(PacketHeader)];
  while (!start_ack) {
    // send start header
    //cout << "here" << endl;
    int num_sent = sendto(socket_, &start_header, sizeof(start_header), 0, (struct sockaddr*) &receiver_info, slen);
    //cout << num_sent << endl;
    *log_file_ << start_header.type << " " << start_header.seqNum << " " << start_header.length << " " << start_header.checksum << endl;

    // start counting time
    gettimeofday(&start_time, NULL);
    gettimeofday(&current_time, NULL);


    // 500 ms = 500000 us
    while (get_us_diff(current_time, start_time) < 500000) {
      // receive ack header
      struct timeval rem;
      rem.tv_sec = 0;
      rem.tv_usec = 500000 - get_us_diff(current_time, start_time);
      if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &rem, sizeof(rem)) < 0) {
        cerr << "Error: bad setsockopt SO_RCVTIMEO\n";
      }
      int num_recvd = recvfrom(socket_, buffer, sizeof(PacketHeader), 0, (struct sockaddr*) &receiver_info, &slen);
      gettimeofday(&current_time, NULL);
      int cur_diff = get_us_diff(current_time, start_time);
      // errno will be set if recvfrom block for 500ms+
      if (num_recvd == -1) {
        // break without setting start_ack, so it will resend start header
        if (cur_diff > 500000) {
          break;
        }
        else {
          gettimeofday(&current_time, NULL);
          continue;
        }
      }

      // copy the buffer in to a header struct
      struct PacketHeader ack_header;
      memcpy(&ack_header, buffer, sizeof(ack_header));

      // logging happens before checking
      *log_file_ << ack_header.type << " " << ack_header.seqNum << " " << ack_header.length << " " << ack_header.checksum << endl;
      // ack should be a packet header, so no ack message body will be expected from the receiver
      // verify message type, sequence number and checksum
      if (ack_header.checksum == 0 && ack_header.type == 3 && ack_header.seqNum == start_header.seqNum) {
        start_ack = true;
        break;
      }
      // update current_time if header verification fails
      gettimeofday(&current_time, NULL);
    }
  }

  //cout << "here2" << endl;


  //------------------------------
  // int temptemp = 10;
  // char temp_buffer[sizeof(header_container_[temptemp]) + buffer_container_[temptemp].size()];
  // memset(temp_buffer, 0, sizeof(temp_buffer));
  // memcpy(temp_buffer, &(header_container_[temptemp]), sizeof(header_container_[temptemp]));
  // memcpy(temp_buffer + sizeof(header_container_[temptemp]),&(buffer_container_[temptemp][0]), buffer_container_[temptemp].size() );
  // temp_buffer[15] = temp_buffer[16];
  // // cout << "tempbuff: " << temp_buffer << endl;
  // int num_sent = sendto(socket_, temp_buffer, sizeof(temp_buffer), 0, (struct sockaddr*) &receiver_info, slen);
  // *log_file_ << header_container_[temptemp].type << " " << header_container_[temptemp].seqNum << " " << header_container_[temptemp].length << " " << header_container_[temptemp].checksum << endl;
  //------------------------------




  // send body packets
  // the body sending process ends when sender window's start goes over boundary

  while (sender_window_start_idx_ < header_container_.size()) {
    int next_max_timeout = 500000;
    for (int i = sender_window_start_idx_; i < sender_window_end_idx_; i++) {
      timeval current_time;
      gettimeofday(&current_time, NULL);
      if (!info_container_[i].acked && (info_container_[i].last_send_time.tv_sec == 0 && info_container_[i].last_send_time.tv_usec == 0 || get_us_diff(current_time, info_container_[i].last_send_time) >= 500000)) {
        char temp_buffer[sizeof(header_container_[i]) + buffer_container_[i].size()];
        memset(temp_buffer, 0, sizeof(temp_buffer));
        memcpy(temp_buffer, &(header_container_[i]), sizeof(header_container_[i]));
        memcpy(temp_buffer + sizeof(header_container_[i]),&(buffer_container_[i][0]), buffer_container_[i].size() );
        // cout << "tempbuff: " << temp_buffer << endl;
        int num_sent = sendto(socket_, temp_buffer, sizeof(temp_buffer), 0, (struct sockaddr*) &receiver_info, slen);
        timeval temporary;
        gettimeofday(&temporary, NULL);
        memcpy(&info_container_[i].last_send_time, &temporary, sizeof(timeval));
        *log_file_ << header_container_[i].type << " " << header_container_[i].seqNum << " " << header_container_[i].length << " " << header_container_[i].checksum << endl;
      }
      else if (!info_container_[i].acked && (info_container_[i].last_send_time.tv_sec == 0 && info_container_[i].last_send_time.tv_usec == 0 || get_us_diff(current_time, info_container_[i].last_send_time) < 500000)) {
        if (info_container_[i].last_send_time.tv_sec != 0 && info_container_[i].last_send_time.tv_usec !=0) {
          int elapsed = get_us_diff(current_time, info_container_[i].last_send_time);
          next_max_timeout = min(next_max_timeout, 500000 - elapsed);
        }
      }

    }//for

    // cout << "here3" << endl;
    struct timeval rem;
    rem.tv_sec = 0;
    rem.tv_usec = next_max_timeout;
    if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &rem, sizeof(rem)) < 0) {
      cerr << "Error: bad setsockopt SO_RCVTIMEO\n";
    }
    timeval start_time;
    timeval current_time;
    gettimeofday(&start_time, NULL);
    int num_recvd = recvfrom(socket_, buffer, sizeof(PacketHeader), 0, (struct sockaddr*) &receiver_info, &slen);
    gettimeofday(&current_time, NULL);
    int cur_diff = get_us_diff(current_time, start_time);
    //cout << "here4" << endl;
    //cout << "num_recvd: " << num_recvd << endl;
    //cout << "log(-1) failed: " << std::strerror(errno) << endl;
    if (num_recvd == -1) {
      continue;
    }
    //cout << "here5" << endl;

    struct PacketHeader ack_header;
    memcpy(&ack_header, buffer, sizeof(ack_header));
    *log_file_ << ack_header.type << " " << ack_header.seqNum << " " << ack_header.length << " " << ack_header.checksum << endl;
    //cout << "recvd something" << endl;
    //cout << ack_header.type << endl;
    //cout << ack_header.checksum << endl;
    if (ack_header.checksum == 0 && ack_header.type == 3 && ack_header.seqNum != start_header.seqNum) {
      //cout << "recvd Ack" << endl;
      // time to move foward
      if (ack_header.seqNum == sender_window_start_idx_) {
        // set ack of the packet to True
        info_container_[sender_window_start_idx_].acked = true;
        // find next packet that is not acked
        int i = sender_window_start_idx_;
        while (info_container_[i].acked){
          i += 1;
        }
        vector<PacketHeader>::size_type proposed_start = i;
        sender_window_start_idx_ = min(header_container_.size(), proposed_start);
        vector<PacketHeader>::size_type proposed_end = sender_window_start_idx_ + window_size_;
        sender_window_end_idx_ = min(header_container_.size(), proposed_end);
      }
      else {
        info_container_[ack_header.seqNum].acked = true;
      }
    }
  }//outer while

  // cout << "here6" << endl;


  // set up the END packet
  struct PacketHeader end_header;
  end_header.type = 1;
  end_header.seqNum = start_header.seqNum;
  end_header.length = 0;
  end_header.checksum = 0;

  // send end packet
  // end action switch
  bool end_ack = false;
  while(!end_ack) {
    //cout << "here7" << endl;
    int num_sent = sendto(socket_, &end_header, sizeof(end_header), 0, (struct sockaddr*) &receiver_info, slen);
    *log_file_ << end_header.type << " " << end_header.seqNum << " " << end_header.length << " " << end_header.checksum << endl;
    gettimeofday(&start_time, NULL);
    gettimeofday(&current_time, NULL);

    while (get_us_diff(current_time, start_time) < 500000) {
      struct timeval rem;
      rem.tv_sec = 0;
      rem.tv_usec = 500000 - get_us_diff(current_time, start_time);
      if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &rem, sizeof(rem)) < 0) {
        cerr << "Error: bad setsockopt SO_RCVTIMEO\n";
      }
      int num_recvd = recvfrom(socket_, buffer, sizeof(PacketHeader), 0, (struct sockaddr*) &receiver_info, &slen);
      gettimeofday(&current_time, NULL);
      int cur_diff = get_us_diff(current_time, start_time);
      if (num_recvd == -1) {
        if (cur_diff > 500000) {
          break;
        }
        else {
          gettimeofday(&current_time, NULL);
          continue;
        }
      }

      struct PacketHeader ack_header;
      memcpy(&ack_header, buffer, sizeof(ack_header));

      *log_file_ << ack_header.type << " " << ack_header.seqNum << " " << ack_header.length << " " << ack_header.checksum << endl;

      if (ack_header.checksum == 0 && ack_header.type == 3 && ack_header.seqNum == end_header.seqNum) {
        end_ack = true;
        break;
      }
      gettimeofday(&current_time, NULL);
    }
  }
}

// return current time - start time in microseconds
int wSender::get_us_diff(struct timeval& current_time, struct timeval& start_time) {
  int res = (current_time.tv_sec - start_time.tv_sec) * 1000000 + (current_time.tv_usec - start_time.tv_usec);
  return res;
}
