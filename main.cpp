#include <iostream>
#include <curl/curl.h>
#include <json/json.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include <unistd.h>

// ZRH Airport coordinates
const double ZRH_LAT = 47.458056;
const double ZRH_LON = 8.548056;

struct Aircraft {
    std::string flightNumber;
    double longitude;
    double latitude;
    double altitude;
    double distance;
};

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t totalSize = size * nmemb;
    response->append((char*)contents, totalSize);
    return totalSize;
}

double calculateDistance(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371.0;
    double lat1_rad = lat1 * M_PI / 180.0;
    double lon1_rad = lon1 * M_PI / 180.0;
    double lat2_rad = lat2 * M_PI / 180.0;
    double lon2_rad = lon2 * M_PI / 180.0;
    
    double dlat = lat2_rad - lat1_rad;
    double dlon = lon2_rad - lon1_rad;
    
    double a = sin(dlat/2) * sin(dlat/2) + 
               cos(lat1_rad) * cos(lat2_rad) * 
               sin(dlon/2) * sin(dlon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    
    return R * c;
}

std::vector<Aircraft> getClosestAircraft() {
    CURL* curl;
    CURLcode res;
    std::string response;
    std::vector<Aircraft> aircraftList;
    
    curl = curl_easy_init();
    if(!curl) return aircraftList;
    
    curl_easy_setopt(curl, CURLOPT_URL, "https://opensky-network.org/api/states/all");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "ADSB-Simple/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if(res != CURLE_OK) return aircraftList;
    
    Json::CharReaderBuilder readerBuilder;
    Json::Value root;
    std::string errors;
    std::istringstream responseStream(response);
    
    if(!Json::parseFromStream(readerBuilder, responseStream, &root, &errors)) {
        return aircraftList;
    }
    
    Json::Value states = root["states"];
    if(!states.isArray()) return aircraftList;
    
    for(const auto& ac : states) {
        if(ac.isArray() && ac.size() >= 14) {
            double lon = ac[5].asDouble();
            double lat = ac[6].asDouble();
            
            if(lon >= -180 && lon <= 180 && lat >= -90 && lat <= 90) {
                Aircraft aircraft;
                aircraft.flightNumber = ac[1].isString() ? ac[1].asString() : "N/A";
                aircraft.longitude = lon;
                aircraft.latitude = lat;
                aircraft.altitude = ac[13].isNumeric() ? ac[13].asDouble() : 0.0;
                aircraft.distance = calculateDistance(ZRH_LAT, ZRH_LON, lat, lon);
                
                if(aircraft.flightNumber != "N/A" && aircraft.altitude > 0) {
                    aircraftList.push_back(aircraft);
                }
            }
        }
    }
    
    // Sort by distance and keep only 10 closest
    std::sort(aircraftList.begin(), aircraftList.end(), 
              [](const Aircraft& a, const Aircraft& b) { return a.distance < b.distance; });
    
    if(aircraftList.size() > 10) {
        aircraftList.resize(10);
    }
    
    return aircraftList;
}

int main() {
    std::cout << "Tracking 10 closest aircraft to ZRH Airport" << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;
    std::cout << "=============================================" << std::endl;
    
    while(true) {
        std::vector<Aircraft> aircraft = getClosestAircraft();
        
        // Clear screen and move cursor to top
        std::cout << "\033[2J\033[1;1H";
        
        std::cout << "10 closest aircraft to ZRH" << std::endl;
        std::cout << "Format: Flight, Longitude, Latitude, Altitude(m)" << std::endl;
        std::cout << "=============================================" << std::endl;
        
        for(const auto& ac : aircraft) {
            std::cout << ac.flightNumber << ", " 
                      << ac.longitude << ", " 
                      << ac.latitude << ", " 
                      << ac.altitude << std::endl;
        }
        
        std::cout << "=============================================" << std::endl;
        sleep(1);
    }
    
    return 0;
}