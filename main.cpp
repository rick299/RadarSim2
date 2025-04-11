#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <thread>
#include <chrono>
#include <cstdlib>  // For std::system
#include <nlohmann/json.hpp> // Include the JSON library
#include <boost/asio.hpp>    // Include Boost ASIO for TCP communication

using json = nlohmann::json;
using boost::asio::ip::tcp;

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

void displayRadarObjects(const std::vector<RadarObject>& objects) {
    for (const auto& obj : objects) {
        std::cout << "Radar Object: " << obj.timestamp
                  << ", Sensor: " << obj.sensor
                  << ", X: " << obj.X << ", Y: " << obj.Y << ", Z: " << obj.Z
                  << ", Range: " << obj.Range << ", Azimuth: " << obj.Az << ", Elevation: " << obj.El
                  << "\n";
    }
}

std::vector<RadarObject> parseRadarData(const std::string& jsonData) {
    std::vector<RadarObject> radarObjects;
    try {
        auto jsonFrame = json::parse(jsonData);
        for (const auto& obj : jsonFrame["objects"]) {
            RadarObject radarObj;
            radarObj.timestamp = obj["timestamp"];
            radarObj.sensor = obj["sensor"];
            radarObj.src = obj["src"];
            radarObj.X = obj["X"];
            radarObj.Y = obj["Y"];
            radarObj.Z = obj["Z"];
            radarObj.Xdir = obj["Xdir"];
            radarObj.Ydir = obj["Ydir"];
            radarObj.Zdir = obj["Zdir"];
            radarObj.Range = obj["Range"];
            radarObj.RangeRate = obj["RangeRate"];
            radarObj.Pwr = obj["Pwr"];
            radarObj.Az = obj["Az"];
            radarObj.El = obj["El"];
            radarObj.ID = obj["ID"];
            radarObj.Xsize = obj["Xsize"];
            radarObj.Ysize = obj["Ysize"];
            radarObj.Zsize = obj["Zsize"];
            radarObj.Conf = obj["Conf"];
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

    // Wait for the Python bridge to initialize (you can increase the wait time if needed)
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

            auto radarObjects = parseRadarData(jsonData);
            displayRadarObjects(radarObjects);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }

    return 0;
}