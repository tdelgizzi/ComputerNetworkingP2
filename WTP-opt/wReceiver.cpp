#include "wReceiver.h"
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
#include <algorithm>

using namespace std;



wReceiver::wReceiver(int port_num_,  unsigned int window_size_, std::string output_dir_, std::ofstream* log_file_) :
port_num(port_num_), window_size(window_size_), output_dir(output_dir_), log_file(log_file_) {}


bool rPackComp (recvPacket i,recvPacket j) { return (i.seqNum<j.seqNum); }

wReceiver::~wReceiver() {
  if (sock != -1) {
    close(sock);
  }
    //TODO
    /*
  if (output_file->is_open()) {
    output_file->close();
  }
    */
  if (log_file->is_open()) {
    log_file->close();
  }
}

bool wReceiver::SetupSocket(){
    //cout << "Setting up Socket" << endl;
    //create a UDP socket
    struct sockaddr_in si_me;
    if ((sock =socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        perror("socket");
        exit(1);
    }
    // zero out the structure
    memset((char *) &si_me, 0, sizeof(si_me));

    //si_me variables
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(port_num);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    //bind socket to port
    if( ::bind(sock , (struct sockaddr*)&si_me, sizeof(si_me) ) == -1)
    {
            perror("bind");
            exit(1);
    }
    return true;
}

bool wReceiver::Start_Listening(){
    //cout << "Waiting for data on port %d ..." << port_num << endl;

    struct sockaddr_in si_other;
    slen = sizeof(si_other);

    int connection_num = -1;
    bool has_connection = false;
    unsigned int my_checksum;
    unsigned int expected_seqNum = 0;
    int start_seqNum = -1;
    int last_ordered_index = -1;
    vector<recvPacket> rPackets;

    //filesystem::create_directory(output_dir);
    ofstream output_stream;
    string cur_output_file;
    //keep listening for data
    while(1)
        {
            //cout << "here (start of while(1))" << endl;
            //try to receive some data, this is a blocking call
            if ((recv_len = recvfrom(sock, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen)) == -1){
                perror("recvfrom()");
                exit(1);
            }
            //cout <<"recv_len: " << recv_len << endl;
            //printf("Received packet from %s:%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
            //cout << "BP1" << endl;
            //DECODE PACKET HEADER from buf
            struct PacketHeader recv_header;
            memcpy(&recv_header, buf, sizeof(recv_header));
            //cout << "BP2" << endl;
            //TODO
            //SET DATA INTO A CSTRING or something
            char data[(recv_len - sizeof(recv_header))];
            memcpy(&data, buf + sizeof(recv_header), sizeof(data));
            // data[sizeof(data)] = '\0';
            //cout << "BP3" << endl;
            //printf("Data: %s\n" , data);
            //fflush(stdout);
            //cout << "recv_header_type: " << recv_header.type << endl;
            //cout << "recv_header_checksum: " << recv_header.checksum << endl;
           // cout << "recv_header_seqnum: " << recv_header.seqNum << endl;

            //LOG BEFORE EVERYTHING ELSE
            *log_file << recv_header.type << " " << recv_header.seqNum << " " << recv_header.length << " " << recv_header.checksum << endl;
            //create ack_header
            struct PacketHeader ack_header;
            ack_header.type = 3;
            ack_header.length = 0;
            ack_header.checksum = 0;
            ack_header.seqNum = -1;

            //Check for START and set has_connection if needed
            if (recv_header.type == 0 && (start_seqNum == -1 || start_seqNum == (int) recv_header.seqNum)){
                //cout << "here (start of START)" << endl;
                start_seqNum = (int) recv_header.seqNum;
                has_connection = true;
                connection_num++;
                expected_seqNum = 0;
                last_ordered_index = -1;

                //todo
                //Send back ACK with same seqNum
                ack_header.seqNum = recv_header.seqNum;
                int num_sent = sendto(sock, &ack_header, sizeof(ack_header), 0, (struct sockaddr*) &si_other, slen);
                //cout << "start_num_sent: " << num_sent << endl;

                //LOG what is being sent back
                *log_file << ack_header.type << " " << ack_header.seqNum << " " << ack_header.length << " " << ack_header.checksum << endl;


                //open a new file in the output_dir named file-connection_num.out
                string output_file_name;
                output_file_name += output_dir;
                output_file_name += "/FILE-";
                output_file_name += to_string(connection_num);
                output_file_name += ".out";
                cur_output_file = output_file_name;
                //cout << "curOfile: " << cur_output_file << endl;
                //output_stream.clear();
                if (!output_stream.is_open()) {
                  output_stream.open(cur_output_file, ios::trunc);
                }
                //output_stream << "hello world" << endl;

                //output_stream.close();

                //** MAY NEED TO MANUALLY REMOVE LEADING SLASH FROM OUTPUR-dir


            }




            //IF DATA
            if(recv_header.type == 2 && has_connection){
                //cout << "here (start of DATA)" << endl;
                //calculate my_checksum using crc32() and buf
                my_checksum = crc32(data, sizeof(data));
                //cout << "my_checksum: " << my_checksum << endl;
                //****
                //should this be recv_header.length instead of sizeof(Data)
                //****


                //format for storing recived packets that have data
                recvPacket temp;
                temp.seqNum = recv_header.seqNum;
                memcpy(temp.data, data, sizeof(data));
                temp.length = recv_header.length;
                //todo
                //add to a vector
                //print using the same method as the log

                //if my_checksum != sender_checksum
                if (my_checksum != recv_header.checksum || recv_header.seqNum >= expected_seqNum + window_size){
                    //Drop packet -> dont send ACK
                    continue;
                }
                else {
                    rPackets.push_back(temp);
                    sort(rPackets.begin(), rPackets.end(), rPackComp);
                    for (auto it = rPackets.begin(); it != rPackets.end();) {
                        if(it->seqNum == last_ordered_index + 1){
                            last_ordered_index = it->seqNum;
                            output_stream.write(it->data, it->length);
                            // output_stream.flush();
                            rPackets.erase(it);
                        }
                        else{
                            it++;
                        }
                    }
                    //seqnum Check
                    //If it receives a packet with seqNum not equal to N, it will send back an ACK with seqNum=N
                    if  (expected_seqNum != recv_header.seqNum){
                        ack_header.seqNum = recv_header.seqNum;
                    }
                    //If it receives a packet with seqNum=N, it will check for the highest sequence number (say M) of the in­order packets it has already received and send ACK with seqNum=M+1
                    else if (expected_seqNum == recv_header.seqNum){
                        //need to keep a list of inorder packets, and when N is recived, add it to the output file, then also anything from N to M, then the new N becomes M+1
                        //so could keep track of N, and then also store a struct that contains, seqnum, and data, and have a vector of those.
                        ack_header.seqNum = recv_header.seqNum;
                        expected_seqNum = last_ordered_index + 1;
                    }


                }//else

                int num_sent = sendto(sock, &ack_header, sizeof(ack_header), 0, (struct sockaddr*) &si_other, slen);
               // cout << "data_num_sent: " << num_sent << endl;

                //LOG what is being sent back
                *log_file << ack_header.type << " " << ack_header.seqNum << " " << ack_header.length << " " << ack_header.checksum << endl;

                //TODO
                //If the next expected seqNum is N, wReceiver will drop all packets with seqNum greater than or equal to N + window-size to maintain a window-size window.
                //Add inorder packets to the output dir?
                //really slow apporach until I think of somethin faster
                //store and delete all in order packets
               // cout << "last_ordered_index " << last_ordered_index << endl;
               // cout << "expected_seqNum " << expected_seqNum << endl;
                //delete anything above window size
                for (auto it = rPackets.begin(); it != rPackets.end();) {
                    if(it->seqNum >= expected_seqNum + window_size){
                        rPackets.erase(it);
                    }
                    else if(it->seqNum <= last_ordered_index){
                        rPackets.erase(it);
                    }
                    else{
                        it++;
                    }
                }



            }//if DATA

            //if END
            if(recv_header.type == 1){
                //cout << "here (start of END)" << endl;
                //todo
                //send back ACK with same Seqnum as recv_from
                ack_header.seqNum = recv_header.seqNum;
                int num_sent = sendto(sock, &ack_header, sizeof(ack_header), 0, (struct sockaddr*) &si_other, slen);
                //cout << "End_num_sent: " << num_sent << endl;
                //LOG what is being sent back
                *log_file << ack_header.type << " " << ack_header.seqNum << " " << ack_header.length << " " << ack_header.checksum << endl;


                has_connection = false;
                if (output_stream.is_open()) {
                    output_stream.close();
                }
                start_seqNum = -1;

            }//if end



           // printf("Received packet from %s:%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
            //printf("Data: %s\n" , buf);
            //fflush(stdout);





        }//while 1

}//Start_Listening
