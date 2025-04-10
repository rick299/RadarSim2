#include "flight_control_sample.hpp"
#include "flight_sample.hpp"
#include "dji_linux_helpers.hpp"
#include <pybind11/embed.h> // Include the pybind11 header for embedding Python
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace py = pybind11;

using namespace DJI::OSDK;
using namespace DJI::OSDK::Telemetry;

// Callback function for obtaining joystick control authority
void ObtainJoystickCtrlAuthorityCB(ErrorCode::ErrorCodeType errorCode, UserData userData)
{
  if (errorCode == ErrorCode::FlightControllerErr::SetControlParam::ObtainJoystickCtrlAuthoritySuccess)
  {
    DSTATUS("ObtainJoystickCtrlAuthoritySuccess");
  }
}

// Callback function for releasing joystick control authority
void ReleaseJoystickCtrlAuthorityCB(ErrorCode::ErrorCodeType errorCode, UserData userData)
{
  if (errorCode == ErrorCode::FlightControllerErr::SetControlParam::ReleaseJoystickCtrlAuthoritySuccess)
  {
    DSTATUS("ReleaseJoystickCtrlAuthoritySuccess");
  }
}

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

// Function to deserialize pickled data using Python
FrameDesc deserializePickle(const std::vector<char>& frameData) {
  FrameDesc frame;
  py::scoped_interpreter guard{}; // Start the Python interpreter

  try {
    // Import the Python deserialization module
    py::module_ deserializer = py::module_::import("deserializer");
    py::object result = deserializer.attr("deserialize")(py::bytes(frameData.data(), frameData.size()));

    // Convert the Python object (list of dicts) into the C++ structure
    for (const auto& obj : result) {
      RadarObject radarObj;
      radarObj.timestamp = obj["timestamp"].cast<std::string>();
      radarObj.sensor = obj["sensor"].cast<std::string>();
      radarObj.src = obj["src"].cast<std::string>();
      radarObj.X = obj["X"].cast<float>();
      radarObj.Y = obj["Y"].cast<float>();
      radarObj.Z = obj["Z"].cast<float>();
      radarObj.Xdir = obj["Xdir"].cast<float>();
      radarObj.Ydir = obj["Ydir"].cast<float>();
      radarObj.Zdir = obj["Zdir"].cast<float>();
      radarObj.Range = obj["Range"].cast<float>();
      radarObj.RangeRate = obj["RangeRate"].cast<float>();
      radarObj.Pwr = obj["Pwr"].cast<float>();
      radarObj.Az = obj["Az"].cast<float>();
      radarObj.El = obj["El"].cast<float>();
      radarObj.ID = obj["ID"].cast<std::string>();
      radarObj.Xsize = obj["Xsize"].cast<float>();
      radarObj.Ysize = obj["Ysize"].cast<float>();
      radarObj.Zsize = obj["Zsize"].cast<float>();
      radarObj.Conf = obj["Conf"].cast<float>();

      frame.objects.push_back(radarObj);
    }
  } catch (const std::exception& e) {
    std::cerr << "Failed to deserialize pickled data: " << e.what() << "\n";
  }

  return frame;
}

// Function to display live radar data
void displayLiveRadarData() {
  std::string server_ip = "192.168.10.10"; // Replace with your server IP
  int server_port = 4035;                 // Replace with your server port

  while (true) {
    try {
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
        uint32_t frame_size;
        int bytes_received = recv(client_socket, &frame_size, sizeof(frame_size), 0);
        if (bytes_received <= 0) {
          std::cerr << "Connection closed or error occurred (frame size). Retrying connection...\n";
          break;
        }

        frame_size = ntohl(frame_size);
        std::cout << "Expecting frame size: " << frame_size << " bytes\n";

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

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
  int functionTimeout = 1;
  LinuxSetup linuxEnvironment(argc, argv);
  Vehicle* vehicle = linuxEnvironment.getVehicle();
  if (vehicle == NULL) {
    std::cout << "Vehicle not initialized, exiting.\n";
    return -1;
  }

  vehicle->flightController->obtainJoystickCtrlAuthorityAsync(ObtainJoystickCtrlAuthorityCB, nullptr, functionTimeout, 2);
  FlightSample* flightSample = new FlightSample(vehicle);

  std::cout
      << "| Available commands:                                            |\n"
      << "| [a] Monitored Takeoff + Landing                                |\n"
      << "| [b] Monitored Takeoff + Position Control + Landing             |\n"
      << "| [c] Monitored Takeoff + Position Control + Force Landing       |\n"
      << "| [d] Monitored Takeoff + Velocity Control + Landing             |\n"
      << "| [e] Display Live Radar Data                                    |\n";

  char inputChar;
  std::cin >> inputChar;

  switch (inputChar) {
    case 'a':
      flightSample->monitoredTakeoff();
      flightSample->monitoredLanding();
      break;
    case 'b':
      flightSample->monitoredTakeoff();
      flightSample->moveByPositionOffset((FlightSample::Vector3f){0, 6, 6}, 30, 0.8, 1);
      flightSample->monitoredLanding();
      break;
    case 'e':
      displayLiveRadarData();
      break;
    default:
      break;
  }

  vehicle->flightController->releaseJoystickCtrlAuthorityAsync(ReleaseJoystickCtrlAuthorityCB, nullptr, functionTimeout, 2);
  delete flightSample;
  return 0;
}