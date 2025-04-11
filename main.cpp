#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <thread>
#include <chrono>
#include <cstdlib>  // For std::system
#include <json.hpp> // Include the JSON library
#include <boost/asio.hpp>    // Include Boost ASIO for TCP communication
#include "flight_control_sample.hpp"
#include "flight_sample.hpp"
#include "dji_linux_helpers.hpp"

using namespace DJI::OSDK;
using namespace DJI::OSDK::Telemetry;
using json = nlohmann::json;
using boost::asio::ip::tcp;

struct RadarObject {
    std::string timestamp;  // Changed from float to string to match JSON format
    std::string sensor;
    std::string src;
    float X, Y, Z;
    float Xdir, Ydir, Zdir;
    float Range, RangeRate, Pwr, Az, El;
    std::string ID;
    float Xsize, Ysize, Zsize;
    float Conf;
};

void displayRadarObjects(const std::vector<RadarObject>& objects) {
    for (const auto& obj : objects) {
        std::cout << "Radar Object: "
                  << "timestamp: " << obj.timestamp
                  << ", sensor: " << obj.sensor
                  << ", src: " << obj.src
                  << ", X: " << obj.X << ", Y: " << obj.Y << ", Z: " << obj.Z
                  << ", Range: " << obj.Range << ", Azimuth: " << obj.Az << ", Elevation: " << obj.El
                  << ", Confidence: " << obj.Conf
                  << "\n";
    }
}

std::vector<RadarObject> parseRadarData(const std::string& jsonData) {
    std::vector<RadarObject> radarObjects;
    try {
        auto jsonFrame = json::parse(jsonData);
        for (const auto& obj : jsonFrame["objects"]) {
            RadarObject radarObj;
            radarObj.timestamp = obj.at("timestamp").dump();
            radarObj.sensor = obj.at("sensor").get<std::string>();
            radarObj.src = obj.at("src").get<std::string>();
            radarObj.X = obj.at("X").get<float>();
            radarObj.Y = obj.at("Y").get<float>();
            radarObj.Z = obj.at("Z").get<float>();
            radarObj.Xdir = obj.at("Xdir").get<float>();
            radarObj.Ydir = obj.at("Ydir").get<float>();
            radarObj.Zdir = obj.at("Zdir").get<float>();
            radarObj.Range = obj.at("Range").get<float>();
            radarObj.RangeRate = obj.at("RangeRate").get<float>();
            radarObj.Pwr = obj.at("Pwr").get<float>();
            radarObj.Az = obj.at("Az").get<float>();
            radarObj.El = obj.at("El").get<float>();
            radarObj.ID = obj.at("ID").get<std::string>();
            radarObj.Xsize = obj.at("Xsize").get<float>();
            radarObj.Ysize = obj.at("Ysize").get<float>();
            radarObj.Zsize = obj.at("Zsize").get<float>();
            radarObj.Conf = obj.at("Conf").get<float>();

            radarObjects.push_back(radarObj);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON data: " << e.what() << "\n";
    }
    return radarObjects;
}

void runPythonBridge() {
    std::cout << "Starting Python bridge...\n";
    int result = std::system("python3 python_bridge.py &");
    if (result != 0) {
        std::cerr << "Failed to start Python bridge script. Error code: " << result << "\n";
        exit(EXIT_FAILURE);
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "Python bridge started successfully.\n";
}

void ObtainJoystickCtrlAuthorityCB(ErrorCode::ErrorCodeType errorCode, UserData userData) {
    if (errorCode == ErrorCode::FlightControllerErr::SetControlParam::ObtainJoystickCtrlAuthoritySuccess) {
        DSTATUS("ObtainJoystickCtrlAuthoritySuccess");
    }
}

void ReleaseJoystickCtrlAuthorityCB(ErrorCode::ErrorCodeType errorCode, UserData userData) {
    if (errorCode == ErrorCode::FlightControllerErr::SetControlParam::ReleaseJoystickCtrlAuthoritySuccess) {
        DSTATUS("ReleaseJoystickCtrlAuthoritySuccess");
    }
}

int main(int argc, char** argv) {
    // Initialize variables
    int functionTimeout = 1;

    // Setup OSDK
    LinuxSetup linuxEnvironment(argc, argv);
    Vehicle* vehicle = linuxEnvironment.getVehicle();
    if (vehicle == NULL) {
        std::cout << "Vehicle not initialized, exiting.\n";
        return -1;
    }

    vehicle->flightController->obtainJoystickCtrlAuthorityAsync(ObtainJoystickCtrlAuthorityCB, nullptr, functionTimeout, 2);
    FlightSample* flightSample = new FlightSample(vehicle);

    // Display interactive prompt
    std::cout
        << "| Available commands: |\n"
        << "| [a] Monitored Takeoff + Landing |\n"
        << "| [b] Monitored Takeoff + Position Control + Landing |\n"
        << "| [c] Monitored Takeoff + Position Control + Force Landing |\n"
        << "| [d] Monitored Takeoff + Velocity Control + Landing |\n"
        << "| [e] Radar data processing |\n";

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
            flightSample->moveByPositionOffset({0, 6, 6}, 30, 0.8, 1);
            flightSample->monitoredLanding();
            break;
        }
        case 'c': {
            flightSample->monitoredTakeoff();
            flightSample->moveByPositionOffset({0, 0, 30}, 0, 0.8, 1);
            flightSample->goHomeAndConfirmLanding();
            break;
        }
        case 'd': {
            flightSample->monitoredTakeoff();
            flightSample->velocityAndYawRateCtrl({0, 0, 5.0}, 0, 2000);
            flightSample->monitoredLanding();
            break;
        }
        case 'e': { // Radar data processing
            try {
                runPythonBridge();
                boost::asio::io_context io_context;
                tcp::socket socket(io_context);
                tcp::resolver resolver(io_context);

                auto endpoints = resolver.resolve("127.0.0.1", "5000");
                boost::asio::connect(socket, endpoints);

                std::cout << "Connected to Python bridge.\n";

                while (true) {
                    boost::asio::streambuf buf;
                    boost::asio::read_until(socket, buf, "\n");

                    std::istream is(&buf);
                    std::string jsonData;
                    std::getline(is, jsonData);

                    if (jsonData.empty()) {
                        std::cerr << "Connection closed or empty data received.\n";
                        break;
                    }

                    auto radarObjects = parseRadarData(jsonData);
                    displayRadarObjects(radarObjects);
                }
            } catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << "\n";
            }
            break;
        }
        default:
            std::cout << "Invalid input.\n";
            break;
    }

    vehicle->flightController->releaseJoystickCtrlAuthorityAsync(ReleaseJoystickCtrlAuthorityCB, nullptr, functionTimeout, 2);
    delete flightSample;

    return 0;
}
