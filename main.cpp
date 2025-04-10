#include "flight_control_sample.hpp"
#include "flight_sample.hpp"
#include "dji_linux_helpers.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pickle.h> // Include cpp-pickle library

using namespace DJI::OSDK;
using namespace DJI::OSDK::Telemetry;

// Radar Object Structure
struct RadarObject {
  std::string timestamp;
  std::string sensor;
  std::string src;
  float X, Y, Z;
  float Xdir, Ydir, Zdir;
  float Range, RangeRate, Pwr, Az, El;
  std::string ID;
  float Xsize, Ysize, Zsize;
  float Conf;
};

struct FrameDesc {
  std::vector<RadarObject> objects;
};

// Function to deserialize pickled data
FrameDesc deserializePickle(const std::vector<char>& frameData) {
  FrameDesc frame;

  try {
    // Use cpp-pickle to load the pickled data
    auto unpickler = pickle::loads(frameData.data(), frameData.size());
    auto objects = unpickler["objects"];

    for (size_t i = 0; i < objects.size(); ++i) {
      RadarObject radarObj;
      radarObj.timestamp = objects[i]["timestamp"].str();
      radarObj.sensor = objects[i]["sensor"].str();
      radarObj.src = objects[i]["src"].str();
      radarObj.X = objects[i]["X"].f();
      radarObj.Y = objects[i]["Y"].f();
      radarObj.Z = objects[i]["Z"].f();
      radarObj.Xdir = objects[i]["Xdir"].f();
      radarObj.Ydir = objects[i]["Ydir"].f();
      radarObj.Zdir = objects[i]["Zdir"].f();
      radarObj.Range = objects[i]["Range"].f();
      radarObj.RangeRate = objects[i]["RangeRate"].f();
      radarObj.Pwr = objects[i]["Pwr"].f();
      radarObj.Az = objects[i]["Az"].f();
      radarObj.El = objects[i]["El"].f();
      radarObj.ID = objects[i]["ID"].str();
      radarObj.Xsize = objects[i]["Xsize"].f();
      radarObj.Ysize = objects[i]["Ysize"].f();
      radarObj.Zsize = objects[i]["Zsize"].f();
      radarObj.Conf = objects[i]["Conf"].f();

      frame.objects.push_back(radarObj);
    }
  } catch (const std::exception& e) {
    std::cerr << "Failed to deserialize pickled data: " << e.what() << "\n";
  }

  return frame;
}

// Function to display live radar data
void displayLiveRadarData() {
  // Radar simulation server details
  std::string server_ip = "192.168.10.10"; // Replace with your server IP
  int server_port = 4035;                 // Replace with your server port

  while (true) {
    try {
      // Create a socket and connect to the simulation server
      int client_socket = socket(AF_INET, SOCK_STREAM, 0);
      if (client_socket < 0) {
        std::cerr << "Failed to create socket. Retrying in 5 seconds...\n";
        std::this_thread::sleep_for(std::chrono::seconds(5));
        continue;
      }

      sockaddr_in server_addr;
      server_addr.sin_family = AF_INET;
      server_addr.sin_port = htons(server_port);
      inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);

      if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Failed to connect to radar simulation server. Retrying in 5 seconds...\n";
        close(client_socket);
        std::this_thread::sleep_for(std::chrono::seconds(5));
        continue;
      }

      std::cout << "Connected to radar simulation server. Receiving live radar data...\n";

      while (true) {
        // Receive the size of the incoming frame
        uint32_t frame_size;
        int bytes_received = recv(client_socket, &frame_size, sizeof(frame_size), 0);
        if (bytes_received <= 0) {
          std::cerr << "Connection closed or error occurred (frame size). Retrying connection...\n";
          break;
        }

        // Convert frame size to host byte order
        frame_size = ntohl(frame_size);
        std::cout << "Expecting frame size: " << frame_size << " bytes\n";

        // Read the full frame data
        std::vector<char> frame_data(frame_size);
        size_t total_received = 0;
        while (total_received < frame_size) {
          bytes_received = recv(client_socket, frame_data.data() + total_received, frame_size - total_received, 0);
          if (bytes_received <= 0) {
            std::cerr << "Connection closed or error occurred (frame data). Retrying connection...\n";
            break;
          }
          total_received += bytes_received;
        }

        if (total_received < frame_size) {
          std::cerr << "Incomplete frame received. Retrying connection...\n";
          break;
        }

        std::cout << "Complete frame received (" << total_received << " bytes).\n";

        // Deserialize the pickled frame
        FrameDesc frame = deserializePickle(frame_data);

        // Display the radar data
        for (const auto& obj : frame.objects) {
          std::cout << "Timestamp: " << obj.timestamp << "\n";
          std::cout << "Sensor: " << obj.sensor << "\n";
          std::cout << "Source: " << obj.src << "\n";
          std::cout << "Position (X, Y, Z): (" << obj.X << ", " << obj.Y << ", " << obj.Z << ")\n";
          std::cout << "Direction (Xdir, Ydir, Zdir): (" << obj.Xdir << ", " << obj.Ydir << ", " << obj.Zdir << ")\n";
          std::cout << "Range: " << obj.Range << "\n";
          std::cout << "Range Rate: " << obj.RangeRate << "\n";
          std::cout << "Power: " << obj.Pwr << "\n";
          std::cout << "Azimuth: " << obj.Az << "\n";
          std::cout << "Elevation: " << obj.El << "\n";
          std::cout << "ID: " << obj.ID << "\n";
          std::cout << "Size (Xsize, Ysize, Zsize): (" << obj.Xsize << ", " << obj.Ysize << ", " << obj.Zsize << ")\n";
          std::cout << "Confidence: " << obj.Conf << "\n";
          std::cout << "----------------------------------------\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Adjust for desired refresh rate
      }

      close(client_socket);
    } catch (const std::exception& e) {
      std::cerr << "An error occurred: " << e.what() << ". Retrying in 5 seconds...\n";
      std::this_thread::sleep_for(std::chrono::seconds(5));
    }
  }
}

// Main function
int main(int argc, char** argv) {
  // Initialize variables
  int functionTimeout = 1;

  // Setup OSDK.
  LinuxSetup linuxEnvironment(argc, argv);
  Vehicle* vehicle = linuxEnvironment.getVehicle();
  if (vehicle == NULL) {
    std::cout << "Vehicle not initialized, exiting.\n";
    return -1;
  }

  // Obtain Control Authority
  vehicle->flightController->obtainJoystickCtrlAuthorityAsync(ObtainJoystickCtrlAuthorityCB, nullptr ,functionTimeout, 2);
  FlightSample* flightSample = new FlightSample(vehicle);

  // Display interactive prompt
  std::cout
      << "| Available commands:                                            |"
      << std::endl;
  std::cout << "| [a] Monitored Takeoff + Landing                                |" << std::endl;
  std::cout << "| [b] Monitored Takeoff + Position Control + Landing             |" << std::endl;
  std::cout << "| [c] Monitored Takeoff + Position Control + Force Landing       |" << std::endl;
  std::cout << "| [d] Monitored Takeoff + Velocity Control + Landing             |" << std::endl;
  std::cout << "| [e] Display Live Radar Data                                    |" << std::endl;

  char inputChar;
  std::cin >> inputChar;

  switch (inputChar) {
    case 'a': {
      flightSample->monitoredTakeoff();
      flightSample->monitoredLanding();
      break;
    }
    case 'b': {
      flightSample->monitoredTakeoff();
      DSTATUS("Take off over!\n");
      flightSample->moveByPositionOffset((FlightSample::Vector3f){0, 6, 6}, 30, 0.8, 1);
      DSTATUS("Step 1 over!\n");
      flightSample->moveByPositionOffset((FlightSample::Vector3f){6, 0, -3}, -30, 0.8, 1);
      DSTATUS("Step 2 over!\n");
      flightSample->moveByPositionOffset((FlightSample::Vector3f){-6, -6, 0}, 0, 0.8, 1);
      DSTATUS("Step 3 over!\n");
      flightSample->monitoredLanding();
      break;
    }
    case 'c': {
      flightSample->monitoredTakeoff();
      vehicle->flightController->setCollisionAvoidanceEnabledSync(
          FlightController::AvoidEnable::AVOID_ENABLE, 1);
      flightSample->moveByPositionOffset((FlightSample::Vector3f){0, 0, 30}, 0, 0.8, 1);
      flightSample->setNewHomeLocation();
      flightSample->setGoHomeAltitude(50);
      flightSample->goHomeAndConfirmLanding();
      break;
    }
    case 'd': {
      flightSample->monitoredTakeoff();
      DSTATUS("Take off over!\n");
      flightSample->velocityAndYawRateCtrl((FlightSample::Vector3f){0, 0, 5.0}, 0, 2000);
      flightSample->monitoredLanding();
      break;
    }
    case 'e': {
      displayLiveRadarData();
      break;
    }
    default:
      break;
  }

  vehicle->flightController->releaseJoystickCtrlAuthorityAsync(ReleaseJoystickCtrlAuthorityCB, nullptr ,functionTimeout, 2);
  delete flightSample;
  return 0;
}