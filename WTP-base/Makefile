all: wSender wReceiver

wSender: wSenderDriver.o wSender.o
	g++ -std=c++17 -g -o wSender wSenderDriver.o wSender.o
wSender.o: wSender.cpp wSender.h
	g++ -std=c++17 -g -c wSender.cpp
wSenderDriver.o: wSenderDriver.cpp wSender.h
	g++ -std=c++17 -g -c wSenderDriver.cpp

wReceiver: wReceiverDriver.o wReceiver.o
	g++ -std=c++17 -g -o wReceiver wReceiverDriver.o wReceiver.o
wReceiver.o: wReceiver.cpp wReceiver.h
	g++ -std=c++17 -g -c wReceiver.cpp
wReceiverDriver.o: wReceiverDriver.cpp wReceiver.h
	g++ -std=c++17 -g -c wReceiverDriver.cpp

clean:
	rm -f *.o wSender
	rm -f *.o wReceiver
