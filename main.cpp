#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <thread>
#include <chrono>
#include <cstdlib>  // For std::system
#include <json.hpp> // Include the JSON library
#include <boost/asio.hpp>    // Include Boost ASIO for TCP communication

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
        // Parse the JSON data
        auto jsonFrame = json::parse(jsonData);

        // Debug: Log the parsed JSON data
        std::cout << "Parsed JSON data: " << jsonFrame.dump(4) << "\n";

        for (const auto& obj : jsonFrame["objects"]) {
            // Debug: Log each object being parsed
            std::cout << "Parsing object: " << obj.dump(4) << "\n";

            RadarObject radarObj;
            radarObj.timestamp = obj.at("timestamp").dump();  // Convert to string
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
        // Log parsing errors
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

    // Wait for the Python bridge to initialize
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "Python bridge started successfully.\n";
}

int main() {
    try {
        // Start the Python bridge
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

            // Debug: Log raw JSON data
            std::cout << "Raw JSON data received: " << jsonData << "\n";

            auto radarObjects = parseRadarData(jsonData);
            displayRadarObjects(radarObjects);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }

    return 0;
}
