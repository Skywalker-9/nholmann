g++ -o sender sender.cpp -I /usr/include/nlohmann/ `pkg-config --cflags --libs gstreamer-1.0` -g  &&  g++ -o receiver receiver.cpp -I /usr/include/nlohmann/ `pkg-config --cflags --libs gstreamer-1.0` -g